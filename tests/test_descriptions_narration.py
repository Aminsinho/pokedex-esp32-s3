import json
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def detail_description(identifier):
    raw = (ROOT / f'sd_dataset/data/{identifier:04}.bin').read_bytes()
    assert raw[:4] == b'PKDP' and struct.unpack_from('<H', raw, 6)[0] == identifier
    offset = 63
    en_len = raw[offset]; offset += 1 + en_len
    es_len = raw[offset]; offset += 1
    return raw[offset:offset + es_len].decode('utf-8')

def test_all_present_spanish_text_is_clean_utf8():
    records = json.loads((ROOT / 'backend/app/data/pokemon.json').read_text(encoding='utf-8'))
    assert len(records) == 1025
    for pokemon in records:
        text = pokemon.get('description_es') or ''
        assert text, f"missing Spanish description #{pokemon['id']}"
        assert not any(mark in text for mark in ('Ã', 'Â', '�'))

def test_accents_survive_detail_generation():
    assert 'Pokémon' in detail_description(1)
    assert 'energía' in detail_description(3)
    assert 'músculos' in detail_description(26)
    assert 'ígnea' in detail_description(392)

def test_spanish_font_and_scanned_narration_hook():
    detail = (ROOT / 'src/pokedex/PokemonDetailScreen.cpp').read_text(encoding='utf-8')
    main = (ROOT / 'src/pokedex/pokedex.ino').read_text(encoding='utf-8')
    assert '&lv_font_spanish_14' in detail
    assert 'AudioManager::playNarration(id);' in main
    assert main.index('AudioManager::playNarration(id);') > main.index('ScanService::phase() == ScanPhase::SHOWING')

def test_pikachu_narration_contract():
    path = ROOT / 'sd_dataset/audio/narration/0025.wav'
    assert path.stat().st_size <= 1048576
    with wave.open(str(path), 'rb') as audio:
        assert (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) == (1, 2, 44100)
        assert 2 < audio.getnframes() / audio.getframerate() < 15
