"""Convert PNG box art to the launcher's SCV1 RGB565 format."""

import argparse
import sys
from pathlib import Path
from struct import pack, pack_into
from tempfile import NamedTemporaryFile

from PIL import Image


MAX_WIDTH = 99
MAX_HEIGHT = 150
CASE_BACKGROUND = (0x1C, 0x2A, 0x37, 255)


def convert(source: Path) -> Path:
    if source.suffix.lower() != ".png":
        raise ValueError("expected a .png file")

    with Image.open(source, formats=("PNG",)) as image:
        width, height = image.size
        width, height = (
            max(1, min(MAX_WIDTH, width * MAX_HEIGHT // height)),
            max(1, min(MAX_HEIGHT, height * MAX_WIDTH // width)),
        )
        art = image.convert("RGBA").resize((width, height), Image.Resampling.LANCZOS)

    background = Image.new("RGBA", art.size, CASE_BACKGROUND)
    rgb = Image.alpha_composite(background, art).convert("RGB").tobytes()
    pixels = bytearray(width * height * 2)
    for pixel in range(width * height):
        red, green, blue = rgb[pixel * 3 : pixel * 3 + 3]
        value = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
        pack_into("<H", pixels, pixel * 2, value)

    destination = source.with_suffix(".cover")
    with NamedTemporaryFile(dir=destination.parent, delete=False) as temporary:
        temp_path = Path(temporary.name)
        try:
            temporary.write(pack("<4sHH", b"SCV1", width, height))
            temporary.write(pixels)
            temporary.close()
            temp_path.replace(destination)
        finally:
            temp_path.unlink(missing_ok=True)
    return destination


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("png", type=Path, nargs="+", help="PNG files to convert beside their originals")
    args = parser.parse_args()
    failed = False
    for source in args.png:
        try:
            print(convert(source))
        except (OSError, ValueError, Image.DecompressionBombError) as error:
            print(f"{source}: {error}", file=sys.stderr)
            failed = True
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
