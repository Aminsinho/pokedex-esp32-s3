#!/usr/bin/env python3
"""Convierte los PNG del usuario a assets RGB565 26x26 para la SD."""
import struct
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Piedras evolutivas"
OUTPUT = ROOT / "sd_dataset" / "ui" / "stones"
FILES = {
    1: "Piedra agua.png", 2: "Piedra alba.png", 3: "Piedra dia.png",
    4: "Piedra fuego.png", 5: "Piedra hielo.png", 6: "Piedra hoja.png",
    7: "Piedra Lunar.png", 8: "Piedra noche.png", 9: "Piedra solar.png",
    10: "Piedra trueno.png",
}


def fit(source, side):
    image = Image.open(source).convert("RGBA")
    image.thumbnail((side, side), Image.Resampling.LANCZOS)
    return image


def write_asset(identifier, foreground):
    canvas = Image.new("RGBA", (26, 26), (158, 180, 196, 255))
    canvas.alpha_composite(foreground, ((26 - foreground.width) // 2, (26 - foreground.height) // 2))
    pixels = bytearray()
    for red, green, blue, _alpha in canvas.getdata():
        value = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
        pixels += struct.pack("<H", value)
    (OUTPUT / f"{identifier:02d}.r565").write_bytes(struct.pack("<II", 26, 26) + pixels)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for identifier, filename in FILES.items():
        source = fit(SOURCE / filename, 24)
        write_asset(identifier, source)
        print(identifier, filename)

    day = fit(SOURCE / "50px-Día.png", 13)
    night = fit(SOURCE / "50px-Noche.png", 13)
    for identifier, sky, name in ((11, day, "amistad+día"), (12, night, "amistad+noche")):
        combo = Image.new("RGBA", (26, 26), (0, 0, 0, 0))
        draw = ImageDraw.Draw(combo)
        draw.ellipse((3, 5, 13, 15), fill=(237, 79, 120, 255))
        draw.ellipse((10, 5, 20, 15), fill=(237, 79, 120, 255))
        draw.polygon(((3, 10), (20, 10), (12, 23)), fill=(237, 79, 120, 255))
        combo.alpha_composite(sky, (13, 0))
        write_asset(identifier, combo)
        print(identifier, name)
    write_asset(13, day); print(13, "día")
    write_asset(14, night); print(14, "noche")


if __name__ == "__main__":
    main()
