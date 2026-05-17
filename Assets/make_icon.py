#!/usr/bin/env python3
"""Generate Assets/icon_1024.png — a brass knob on dark walnut, matching
Source/VintageLookAndFeel (the app's own knob drawing) for brand coherence.

Pure standard library (no Pillow). Run:  python3 Assets/make_icon.py
The PNG is committed so the build does not depend on running this.
"""
import math, os, struct, zlib

SIZE = 1024

# Palette (same hex as VintageLookAndFeel) -------------------------------------
BODY   = (0x1c, 0x1b, 0x18)
PANEL  = (0x2a, 0x24, 0x16)
RING_L = (0xa8, 0x78, 0x28)   # knob ring, lit
RING_D = (0x45, 0x30, 0x08)   # knob ring, shadowed
CAP_L  = (0xe2, 0xb4, 0x6e)   # cap highlight
CAP_D  = (0x3a, 0x30, 0x20)   # cap base
TEXT   = (0xf0, 0xed, 0xe4)   # indicator line


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def smoothcov(d, edge=1.5):
    """Coverage 1 inside (d<0) → 0 outside, smooth across 'edge' px."""
    t = 0.5 - d / (2.0 * edge)
    if t <= 0.0:
        return 0.0
    if t >= 1.0:
        return 1.0
    return t * t * (3.0 - 2.0 * t)


def over(dst, src, a):
    return tuple(src[i] * a + dst[i] * (1.0 - a) for i in range(3))


cx = cy = SIZE / 2.0
ringR = SIZE * 0.345
capR = ringR * 0.80
specR = capR * 0.30
ang = -math.pi / 2.0          # indicator points straight up
li_x0 = cx + math.cos(ang) * capR * 0.34
li_y0 = cy + math.sin(ang) * capR * 0.34
li_x1 = cx + math.cos(ang) * capR * 0.90
li_y1 = cy + math.sin(ang) * capR * 0.90
li_dx, li_dy = li_x1 - li_x0, li_y1 - li_y0
li_len2 = li_dx * li_dx + li_dy * li_dy
li_hw = SIZE * 0.018          # half width of the indicator line

raw = bytearray()
for y in range(SIZE):
    raw.append(0)             # PNG filter: none
    for x in range(SIZE):
        px, py = x + 0.5, y + 0.5

        # 1. background: subtle vertical walnut gradient
        col = lerp(PANEL, BODY, py / SIZE)

        dx, dy = px - cx, py - cy
        dist = math.hypot(dx, dy)

        # 2. soft drop shadow under the knob
        col = over(col, (0, 0, 0), 0.55 * smoothcov(dist - (ringR + 26.0), 30.0))

        # 3. knob ring (diagonal lit→shadow)
        g = max(0.0, min(1.0, ((dx + dy) / (2.2 * ringR)) + 0.5))
        col = over(col, lerp(RING_L, RING_D, g), smoothcov(dist - ringR))

        # 4. cap
        gc = max(0.0, min(1.0, (((dx) * 0.5 + (dy)) / (2.0 * capR)) + 0.5))
        col = over(col, lerp(CAP_L, CAP_D, gc), smoothcov(dist - capR))

        # 5. specular highlight (upper-left of the cap)
        sdx, sdy = px - (cx - capR * 0.34), py - (cy - capR * 0.40)
        col = over(col, (255, 244, 214),
                   0.55 * smoothcov(math.hypot(sdx, sdy) - specR, capR * 0.5))

        # 6. indicator line (rounded capsule)
        t = 0.0 if li_len2 == 0 else ((px - li_x0) * li_dx + (py - li_y0) * li_dy) / li_len2
        t = max(0.0, min(1.0, t))
        ddx = px - (li_x0 + t * li_dx)
        ddy = py - (li_y0 + t * li_dy)
        col = over(col, (0, 0, 0), 0.6 * smoothcov(math.hypot(ddx, ddy) - (li_hw + 2.0)))
        col = over(col, TEXT, smoothcov(math.hypot(ddx, ddy) - li_hw))

        raw += bytes(int(max(0, min(255, round(c)))) for c in col)


def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))


png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
png += chunk(b"IEND", b"")

out = os.path.join(os.path.dirname(__file__), "icon_1024.png")
with open(out, "wb") as f:
    f.write(png)
print("wrote", out, len(png), "bytes")
