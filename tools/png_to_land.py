#!/usr/bin/env python3
"""
Convert a PNG map to a C header for the radar display.

Usage:  python3 png_to_land.py map.png > src/land_mask.h

The PNG can be any size — it will be resized to 480×480.
Dark pixels (r+g+b < 20) are treated as water.
All other pixels become land coloured #002210.
"""

import sys
from struct import pack

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow required.  Install with: pip install pillow")
    sys.exit(1)

W, H = 480, 480

def main(png_path):
    img = Image.open(png_path).convert("RGB")
    if img.size != (W, H):
        img = img.resize((W, H), Image.LANCZOS)

    pixels = img.load()

    # Build 2-byte-per-pixel RGB565 array (480*480*2 = 460800 bytes)
    buf = bytearray()
    for y in range(H):
        for x in range(W):
            r, g, b = pixels[x, y]
            if r + g + b < 20:
                # Water — transparent / let background show
                buf += b'\x00\x00'   # marker for "no land here"
            else:
                # Land — use the radar land colour #002210
                # RGB565: R=0, G=0x22>>2=8, B=0x10>>3=2 → 0x0102
                buf += pack('<H', 0x0102)

    print(f"/* Auto-generated land mask from {png_path} */")
    print(f"/* {W}x{H} pixels, RGB565 format */")
    print(f"#pragma once")
    print(f"#include <stdint.h>")
    print(f"")
    print(f"#define LAND_MASK_W {W}")
    print(f"#define LAND_MASK_H {H}")
    print(f"#define LAND_MASK_SIZE {len(buf)}")
    print(f"")
    print(f"/* 0x0000 = water (transparent), 0x0102 = land (#002210) */")
    print(f"static const uint8_t land_mask_rgb565[{len(buf)}] = {{")
    for i in range(0, len(buf), 16):
        chunk = buf[i:i+16]
        hexes = ', '.join(f'0x{b:02x}' for b in chunk)
        print(f"    {hexes},")
    print(f"}};")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <map.png>")
        sys.exit(1)
    main(sys.argv[1])
