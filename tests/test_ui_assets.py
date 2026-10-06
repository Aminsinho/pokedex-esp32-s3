"""New UI asset/SD type contract regressions; no backend/model retest."""
import importlib.util
import re
import struct
from pathlib import Path
from PIL import Image
import pytest

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


provision = module("provision", ROOT / "tools/provision_sprites.py")
generator = module("generator", ROOT / "tools/generate_local_db.py")


def test_firmware_type_names_match_sd_generator():
    source = (ROOT / "src/pokedex/PokemonService.cpp").read_text(encoding="utf-8")
    table = source.split("TYPE_NAMES[] = {", 1)[1].split("};", 1)[0]
    names = re.findall(r'"([a-z]*)"', table)
    assert len(names) == 19
    for name, code in generator.TYPE_MAP.items():
        assert names[code] == name


@pytest.mark.parametrize("id,name", [(25, "electric"), (1, "grass"), (6, "fire"), (448, "fighting")])
def test_catalogue_detail_type_contract(id, name):
    data = (ROOT / "sd_dataset/data" / f"{id:04}.bin").read_bytes()
    assert data[:4] == b"PKDP"
    assert data[48] == generator.TYPE_MAP[name]


@pytest.mark.parametrize("side,background", [(48, (227, 235, 240)), (96, (134, 185, 188))])
def test_alpha_composited_before_rgb565_packing(tmp_path, side, background):
    source = tmp_path / "sprite.png"
    image = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    image.putpixel((0, 0), (255, 0, 0, 128))
    image.save(source)
    data = provision.encode(source, side, background)
    assert struct.unpack("<II", data[:8]) == (side, side)
    assert len(data) == 8 + side * side * 2
    expected = Image.alpha_composite(Image.new("RGBA", (side, side), background + (255,)), image)
    for i in [0, 1]:
        r, g, b, _ = expected.getpixel((i, 0))
        assert struct.unpack_from("<H", data, 8 + i*2)[0] == (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def test_reject_wrong_sprite_dimensions(tmp_path):
    path = tmp_path / "bad.png"
    Image.new("RGBA", (1, 1)).save(path)
    with pytest.raises(ValueError, match="dimensions"):
        provision.encode(path, 48, (227, 235, 240))


def test_wire_checksum():
    assert provision.checksum(b"hello") == 0x4f9f2cab


def test_packet_flow_waits_for_ack():
    class Port:
        pending = False
        packets = []
        def write(self, data):
            assert not self.pending
            assert len(data) <= 128
            self.pending = True
            self.packets.append(data)
        def readline(self):
            assert self.pending
            self.pending = False
            return b"SPRITE CHUNK\n"
    port = Port()
    data = bytes(range(256)) + b"tail"
    provision.send_blocks(port, data)
    assert b"".join(port.packets) == data
    assert list(map(len, port.packets)) == [128, 128, 4]


def test_packet_flow_stops_on_error():
    class Port:
        writes = 0
        def write(self, data): self.writes += 1
        def readline(self): return b"SPRITE ERROR payload\n"
    port = Port()
    with pytest.raises(RuntimeError, match="not acknowledged"):
        provision.send_blocks(port, bytes(256))
    assert port.writes == 1
