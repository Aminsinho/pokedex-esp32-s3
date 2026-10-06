"""Focused checks for the first SD cry; no hardware audibility assertion."""
from pathlib import Path
import struct
import wave

ROOT = Path(__file__).resolve().parents[1]

def test_pikachu_pcm_contract():
    path = ROOT / 'sd_dataset/audio/0025.wav'
    raw = path.read_bytes()
    assert 44 < len(raw) <= 524288
    assert raw[:4] == b'RIFF' and raw[8:16] == b'WAVEfmt '
    assert struct.unpack_from('<I', raw, 4)[0] == len(raw) - 8
    assert raw[36:40] == b'data'
    assert struct.unpack_from('<I', raw, 40)[0] == len(raw) - 44
    with wave.open(str(path), 'rb') as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 44100)
        assert 0.1 < wav.getnframes() / 44100 < 5
        samples = struct.unpack('<' + 'h' * wav.getnframes(), wav.readframes(wav.getnframes()))
        assert max(samples) > 100 and min(samples) < -100

def test_no_blocking_boot_beep():
    source = (ROOT / 'src/pokedex/pokedex.ino').read_text(encoding='utf-8')
    assert 'AudioManager::bootBeepTest();' in source
    assert 'delay(700)' not in source
