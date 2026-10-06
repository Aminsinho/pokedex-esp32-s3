"""Download one game cry from PokeAPI and prepare PCM WAV for SD (not TTS)."""
import argparse
import audioop
import io
import wave
from pathlib import Path
import httpx
import soundfile as sf

ROOT = Path(__file__).resolve().parents[1]

def convert(data):
    samples, rate = sf.read(io.BytesIO(data), dtype='int16', always_2d=True)
    # Average in int32 to avoid overflow for stereo source.
    mono = samples.astype('int32').mean(axis=1).astype('<i2').tobytes()
    if rate != 44100: mono, _ = audioop.ratecv(mono, 2, 1, rate, 44100, None)
    output = io.BytesIO()
    with wave.open(output, 'wb') as wav:
        wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(44100)
        wav.writeframes(mono)
    if not 44 < len(output.getvalue()) <= 524288: raise ValueError('cry too long')
    return output.getvalue()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('id', type=int)
    args = parser.parse_args()
    if not 1 <= args.id <= 1025: parser.error('id 1..1025')
    with httpx.Client(timeout=30, follow_redirects=True) as client:
        result = client.get(f'https://pokeapi.co/api/v2/pokemon/{args.id}'); result.raise_for_status()
        url = result.json()['cries']['latest']
        if not url.startswith('https://raw.githubusercontent.com/PokeAPI/cries/'): raise ValueError('unexpected source')
        sound = client.get(url); sound.raise_for_status()
    path = ROOT / 'sd_dataset/audio' / f'{args.id:04}.wav'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(convert(sound.content))
    print(f'{path}: {path.stat().st_size} bytes; source {url}')
