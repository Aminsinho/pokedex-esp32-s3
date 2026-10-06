#include "AudioManager.h"
#include "AudioCodecES8311.h"
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <freertos/queue.h>
#include <atomic>
#include <math.h>

static I2SClass output;
static AudioCodecES8311 codec;
static TaskHandle_t task = nullptr;
static QueueHandle_t commands = nullptr;
static bool ready = false;
static std::atomic<bool> playing{false};
static constexpr size_t MAX_WAV = 1048576;
enum class Kind { TONE, CRY, NARRATION, WAV, STOP };
struct Command { Kind kind; float frequency=0; uint32_t duration=0; int volume=0; bool square=false; uint16_t id=0; uint8_t* wav=nullptr; size_t length=0; };
static int16_t samples[256*2]; // DMA producer, never a large stack allocation

static bool post(const Command& command) {
    return ready && commands && xQueueSend(commands, &command, 0) == pdTRUE;
}
static uint32_t le32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
static bool validWav(const uint8_t* p, size_t n) {
    // Canonical WAV produced by tools/prepare_cry.py: PCM mono, 16-bit, 44100.
    return p && n>=44 && n<=MAX_WAV && !memcmp(p,"RIFF",4) && !memcmp(p+8,"WAVEfmt ",8)
        && le32(p+4)==n-8 && le32(p+16)==16 && p[20]==1 && p[21]==0
        && p[22]==1 && p[23]==0 && le32(p+24)==44100 && le32(p+28)==88200
        && p[32]==2 && p[33]==0 && p[34]==16 && p[35]==0
        && !memcmp(p+36,"data",4) && le32(p+40)==n-44 && (n-44)%2==0;
}
static uint8_t* loadPokemonAudio(uint16_t id, bool narration, size_t& length) {
    char path[64];
    snprintf(path,sizeof(path),narration ? "/pokedex/audio/narration/%04u.wav" : "/pokedex/audio/%04u.wav",id);
    File file = SD_MMC.open(path, FILE_READ);
    if (!file) { Serial.printf("[AUDIO] cry %u missing on SD\n",id); return nullptr; }
    length = file.size();
    if (length<44 || length>MAX_WAV) return nullptr;
    auto* data = static_cast<uint8_t*>(heap_caps_malloc(length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if (!data) return nullptr;
    if (file.read(data,length)!=length || !validWav(data,length)) { heap_caps_free(data); return nullptr; }
    return data;
}
static void audioTask(void*) {
    uint8_t* wav = nullptr; size_t position=0, length=0;
    Command tone{Kind::TONE}; uint32_t toneLeft=0, toneTotal=0; float phase=0;
    for (;;) {
        Command command;
        if (xQueueReceive(commands,&command,0)==pdTRUE) {
            if (command.kind==Kind::STOP) {
                if (wav) heap_caps_free(wav); wav=nullptr; toneLeft=0;
            } else if (command.kind==Kind::TONE) {
                tone=command; toneLeft=toneTotal=command.duration*44100/1000; phase=0;
            } else {
                if (wav) heap_caps_free(wav); wav=nullptr;
                if (command.kind==Kind::CRY || command.kind==Kind::NARRATION)
                    wav=loadPokemonAudio(command.id,command.kind==Kind::NARRATION,length);
                else { wav=command.wav; length=command.length; }
                if (wav && !validWav(wav,length)) { heap_caps_free(wav); wav=nullptr; }
                position=44;
                if (wav) Serial.printf("[AUDIO] PCM start id=%u bytes=%u\n",command.id,unsigned(length-44));
            }
        }
        for (int i=0;i<256;++i) {
            int value=0;
            if (wav) {
                if (position+1<length) {
                    value=int16_t(uint16_t(wav[position]) | uint16_t(wav[position+1])<<8)*45/100;
                    position+=2;
                } else {
                    heap_caps_free(wav); wav=nullptr;
                    Serial.printf("[AUDIO] PCM done; stack=%u\n",unsigned(uxTaskGetStackHighWaterMark(nullptr)));
                }
            }
            if (toneLeft) {
                const float envelope = min(1.0f, min(float(toneTotal-toneLeft)/220, float(toneLeft)/220));
                value += int((tone.square ? (phase<.5f?1.f:-1.f) : sinf(phase*6.2831853f))*envelope*tone.volume*327.67f);
                phase += tone.frequency/44100.f; if (phase>=1) phase-=1;
                --toneLeft;
            }
            value = max(-32768,min(32767,value));
            // Master gain: half the previous output for tones and PCM alike.
            samples[i*2]=samples[i*2+1]=int16_t(value / 2);
        }
        playing = wav || toneLeft;
        if (output.write(reinterpret_cast<uint8_t*>(samples),sizeof(samples)) != sizeof(samples)) vTaskDelay(pdMS_TO_TICKS(5));
    }
}
bool AudioManager::begin(int sampleRate,int volume) {
    if (ready) return true;
    if (sampleRate!=44100) return false;
    pinMode(1,OUTPUT); digitalWrite(1,LOW); // Freenove AP_ENABLE
    output.setPins(5,7,8,-1,4);
    if (!output.begin(I2S_MODE_STD,44100,I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO,I2S_STD_SLOT_LEFT,I2S_ROLE_MASTER)) return false;
    if (!codec.begin(44100,constrain(volume,0,100))) { output.end(); return false; }
    commands=xQueueCreate(4,sizeof(Command));
    ready=commands!=nullptr;
    return ready;
}
void AudioManager::startTask(int core) {
    if (!ready || task) return;
    if (xTaskCreatePinnedToCore(audioTask,"audio",8192,nullptr,2,&task,core)!=pdPASS) ready=false;
}
void AudioManager::playTone(float frequency,uint32_t duration,int16_t volume,bool square) {
    if (frequency<40 || frequency>10000 || duration>5000) return;
    Command c{Kind::TONE}; c.frequency=frequency; c.duration=duration; c.volume=constrain(volume,0,40); c.square=square; post(c);
}
void AudioManager::playCry(uint16_t id) { if (!id || id>1025) return; Command c{Kind::CRY}; c.id=id; post(c); }
void AudioManager::playNarration(uint16_t id) { if (!id || id>1025) return; Command c{Kind::NARRATION}; c.id=id; post(c); }
void AudioManager::playClick() { playTone(1500,24,12); }
void AudioManager::playConfirm() { playTone(880,100,20); }
void AudioManager::playError() { playTone(220,160,20); }
void AudioManager::playScan() { playTone(1200,60,15); }
void AudioManager::stop() { post(Command{Kind::STOP}); }
void AudioManager::setVolume(int volume) { codec.setVolume(constrain(volume,0,100)); }
void AudioManager::bootBeepTest() { playTone(1000,200,25); }
bool AudioManager::isPlaying() { return playing; }
bool AudioManager::isReady() { return ready; }
void AudioManager::playWav(const uint8_t* data,size_t length) {
    if (!validWav(data,length)) return;
    auto* copy=static_cast<uint8_t*>(heap_caps_malloc(length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if (!copy) return; memcpy(copy,data,length);
    Command c{Kind::WAV}; c.wav=copy; c.length=length;
    if (!post(c)) heap_caps_free(copy);
}
void AudioManager::playTTS(const char*,bool) { Serial.println("[AUDIO] TTS not implemented"); }
