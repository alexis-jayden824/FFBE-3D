#!/usr/bin/env python3
"""Build a synthetic unit (invented coloured blocks, no game art) in the game's
source format, convert it with unitsprite.py, and check the converter's output
pixel-for-pixel. Writes the .f3ds for the C++ test.   usage: <out.f3ds> <workdir>"""
import os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "assets"))
import unitsprite
from PIL import Image

out, work = sys.argv[1], sys.argv[2]
os.makedirs(work, exist_ok=True)
UNIT = "900000001"

# 64x64 sheet with three solid blocks.
sheet = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
RED, GREEN, BLUE = (255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 128)
sheet.paste(RED, (0, 0, 16, 24)); sheet.paste(GREEN, (20, 0, 28, 8)); sheet.paste(BLUE, (32, 32, 44, 36))
sheet.save(os.path.join(work, f"unit_anime_{UNIT}.png"))

# Frames: anchor, count, then x,y,flip,blend,opacity,rot,sx,sy,w,h,page per part (top-most first).
cgg = [
    "0,2, -4,-40,1,0,100,0, 20,0,8,8,0,  -8,-24,0,0,100,0, 0,0,16,24,0,",   # frame 0: green (flipped) over red
    "0,1, -8,-24,0,0,100,0, 0,0,16,24,0,",                                     # frame 1: red only
    "0,2, 2,-30,0,1,50,90, 32,32,12,4,0,  -8,-24,0,0,100,0, 0,0,16,24,0,",    # frame 2: additive, half opacity, rotated blue
    "0,0,",                                                                     # frame 3: empty
    "0,1, 0,0,0,0,100,0, 60,60,16,16,0,",                                      # frame 4: out of sheet bounds -> dropped
    "0,1, 0,0,0,0,100,0, 0,0,16,24,0,",                                        # frame 5: never referenced
]
open(os.path.join(work, f"unit_cgg_{UNIT}.csv"), "w", newline="").write("\r\n".join(cgg) + "\r\n")
anims = {"idle": ["0,0,0,6,", "1,0,-2,6,"], "atk": ["0,0,0,4,", "2,-6,0,8,", "4,0,0,2,"], "dead": ["3,0,0,5,"],
         "broken": ["99,0,0,5,"]}                                             # only invalid frames -> no animation
for name, lines in anims.items():
    open(os.path.join(work, f"unit_{name}_cgs_{UNIT}.csv"), "w", newline="").write("\r\n".join(lines) + "\r\n")

files = unitsprite.load_from_dir(work, UNIT)
blob, info, atlas = unitsprite.convert(files, UNIT, 0.5, ["all"])
_, default_info, _ = unitsprite.convert(files, UNIT, 0.5)
assert default_info["anims"] == ["idle", "atk", "dead"], default_info          # default set keeps these
_, only_idle, _ = unitsprite.convert(files, UNIT, 1.0, ["idle"])
assert only_idle["anims"] == ["idle"] and only_idle["scale"] == 1.0 and only_idle["frames"] == 2, only_idle
assert info["atlas"] == (64, 64), info
assert info["anims"] == ["idle", "atk", "dead"], info
assert info["frames"] == 5 and info["rects"] == 3 and info["dropped_parts"] == 1, info
assert info["rotated"] == 1 and info["flipped"] == 1 and info["additive"] == 1, info

# Header + texture: un-swizzle and compare with the atlas the converter built.
magic, ver, W, H, nrect, nframe, npart, nanim, scale, nstep, tex_off, tex_size = struct.unpack_from("<4sIHHHHIHHIII", blob, 0)
assert (magic, ver, W, H, scale, tex_size) == (b"F3DS", 1, 64, 64, 500, 64 * 64 * 4)
tex, px = blob[tex_off:], atlas.load()          # texture is stored bottom row first
for y in range(H):
    for x in range(W):
        m = 0
        for b in range(3):
            m |= ((x >> b) & 1) << (2 * b) | ((y >> b) & 1) << (2 * b + 1)
        o = (((y >> 3) * (W >> 3) + (x >> 3)) * 64 + m) * 4
        a, bb, g, r = tex[o:o + 4]
        assert (r, g, bb, a) == px[x, H - 1 - y], (x, y)
# Scaled blocks keep their colours and sizes (8x12 red, 4x4 green, 6x2 blue).
rects = [struct.unpack_from("<4H", blob, 36 + i * 8) for i in range(nrect)]
sizes = sorted((r[2], r[3]) for r in rects)
assert sizes == [(4, 4), (6, 2), (8, 12)], sizes
for x, y, w, h in rects:
    want = {(8, 12): RED, (4, 4): GREEN, (6, 2): BLUE}[(w, h)]
    assert all(px[x + i, y + j] == want for i in range(w) for j in range(h))

open(out, "wb").write(blob)
print("sprite fixture ok")
