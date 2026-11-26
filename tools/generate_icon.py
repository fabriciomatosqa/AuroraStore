#!/usr/bin/env python3
"""Generate a simple 512x512 RGB PNG icon without third-party dependencies."""
import struct
import zlib

W = H = 512
rows = []
for y in range(H):
    row = bytearray([0])
    for x in range(W):
        t = (x + y) / (W + H - 2)
        r, g, b = int(12 + 24*t), int(18 + 18*t), int(42 + 70*t)
        inside = (180 <= x <= 332 and 130 <= y <= 382 and
                  abs(x - 256) < (y - 105) * 0.62 and
                  not (250 <= y <= 295 and 210 <= x <= 302))
        if inside:
            r, g, b = 65, 210, 255
        row.extend((r, g, b))
    rows.append(bytes(row))

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

raw = b"".join(rows)
png = (b"\x89PNG\r\n\x1a\n" +
       chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) +
       chunk(b"IDAT", zlib.compress(raw, 9)) +
       chunk(b"IEND", b""))
with open("sce_sys/icon0.png", "wb") as f:
    f.write(png)
