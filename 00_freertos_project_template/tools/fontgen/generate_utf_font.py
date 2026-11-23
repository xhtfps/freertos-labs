from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

MAGIC = b"UTF1"
VERSION = 1


def build_canvas(glyph: dict, size: int) -> list[list[int]]:
    canvas = [[0 for _ in range(size)] for _ in range(size)]

    pixels = glyph["pixels"]
    rows = len(pixels)
    cols = len(pixels[0]) if rows else 0
    bitmap_left = glyph["freetype"]["bitmap_left"]
    bitmap_top = glyph["freetype"]["bitmap_top"]
    ascent = glyph["_meta_ascent"]
    top = ascent - bitmap_top

    for y in range(rows):
        for x in range(cols):
            dst_x = bitmap_left + x
            dst_y = top + y
            if 0 <= dst_x < size and 0 <= dst_y < size and pixels[y][x] > 0:
                canvas[dst_y][dst_x] = 1

    return canvas


def pack_column_major(canvas: list[list[int]]) -> bytes:
    size = len(canvas)
    out = bytearray()
    blocks = (size + 7) // 8

    for x in range(size):
        for block in range(blocks):
            value = 0
            for bit in range(8):
                y = block * 8 + bit
                if y < size and canvas[y][x]:
                    value |= 0x80 >> bit
            out.append(value)

    return bytes(out)


def main() -> None:
    parser = argparse.ArgumentParser(description="Pack LVGL dump font_info.json into W25Q64 UTF font binary")
    parser.add_argument("--input", required=True, help="Path to lv_font_conv font_info.json")
    parser.add_argument("--output", required=True, help="Output binary path")
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    data = json.loads(input_path.read_text(encoding="utf-8"))
    size = int(data["size"])
    ascent = int(data["ascent"])
    glyphs = data["glyphs"]
    bytes_per_glyph = size * ((size + 7) // 8)

    entries: list[bytes] = []
    for glyph in glyphs:
        glyph["_meta_ascent"] = ascent
        codepoint = int(glyph["code"])
        canvas = build_canvas(glyph, size)
        bitmap = pack_column_major(canvas)
        entries.append(struct.pack(">I", codepoint) + bitmap)

    header = struct.pack(
        "<4sHHHHHI",
        MAGIC,
        VERSION,
        size,
        size,
        bytes_per_glyph,
        0,
        len(entries),
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("wb") as fp:
        fp.write(header)
        for entry in entries:
            fp.write(entry)

    print(f"Wrote {len(entries)} glyphs to {output_path}")
    print(f"Glyph cell: {size}x{size}, bytes/glyph: {bytes_per_glyph}")


if __name__ == "__main__":
    main()
