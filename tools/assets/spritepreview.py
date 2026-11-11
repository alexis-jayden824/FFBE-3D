#!/usr/bin/env python3
"""Render frames of a converted .f3ds sprite to a PNG contact sheet on your PC.

This draws from the console file itself (it un-swizzles the atlas and applies
the same placement rules the console uses), so it is the check that a
conversion came out right before copying anything to the SD card.

    python tools/assets/spritepreview.py party/1.f3ds -o preview.png
    python tools/assets/spritepreview.py party/1.f3ds -o atk.png --anim atk
"""
import argparse, struct, sys

try:
    from PIL import Image, ImageChops
except ImportError:
    sys.exit("This tool needs Pillow. Install it with:  python -m pip install pillow")


def load(path):
    b = open(path, "rb").read()
    magic, ver, W, H, nrect, nframe, npart, nanim, scale, nstep, tex_off, tex_size = struct.unpack_from("<4sIHHHHIHHIII", b, 0)
    if magic != b"F3DS" or ver != 1:
        sys.exit(f"{path}: not a .f3ds sprite")
    o = 36
    rects = [struct.unpack_from("<4H", b, o + i * 8) for i in range(nrect)]; o += nrect * 8
    frames = [struct.unpack_from("<IHH", b, o + i * 8)[:2] for i in range(nframe)]; o += nframe * 8
    parts = [struct.unpack_from("<Hhhh4B", b, o + i * 12) for i in range(npart)]; o += npart * 12
    anims = []
    for i in range(nanim):
        name, first, n, _ = struct.unpack_from("<16sIHH", b, o + i * 24)
        anims.append((name.split(b"\0")[0].decode(), first, n))
    o += nanim * 24
    steps = [struct.unpack_from("<HhhH", b, o + i * 8) for i in range(nstep)]
    px = bytearray(W * H * 4)
    tex = b[tex_off:tex_off + tex_size]
    for y in range(H):
        for x in range(W):
            m = 0
            for k in range(3):
                m |= ((x >> k) & 1) << (2 * k) | ((y >> k) & 1) << (2 * k + 1)
            s = (((y >> 3) * (W >> 3) + (x >> 3)) * 64 + m) * 4
            d = (y * W + x) * 4
            px[d], px[d + 1], px[d + 2], px[d + 3] = tex[s + 3], tex[s + 2], tex[s + 1], tex[s]
    return dict(atlas=Image.frombytes("RGBA", (W, H), bytes(px)).transpose(Image.FLIP_TOP_BOTTOM), scale=scale, rects=rects, frames=frames, parts=parts, anims=anims, steps=steps)


def draw_frame(s, frame, canvas, ox, oy):
    """Composite one frame with its origin at (ox, oy). Mirrors the console renderer."""
    first, n = s["frames"][frame]
    for rect, dx, dy, rot, flip, blend, opacity, _ in s["parts"][first:first + n]:
        x, y, w, h = s["rects"][rect]
        img = s["atlas"].crop((x, y, x + w, y + h))
        if flip & 1: img = img.transpose(Image.FLIP_LEFT_RIGHT)
        if flip & 2: img = img.transpose(Image.FLIP_TOP_BOTTOM)
        if rot: img = img.rotate(-rot, expand=True, resample=Image.NEAREST)
        if opacity < 255:
            img.putalpha(img.getchannel("A").point(lambda a: a * opacity // 255))
        layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        layer.paste(img, (ox + dx, oy + dy))
        if blend == 1:
            rgb = Image.composite(layer, Image.new("RGBA", canvas.size, (0, 0, 0, 0)), layer.getchannel("A")).convert("RGB")
            merged = ImageChops.add(canvas.convert("RGB"), rgb)
            merged.putalpha(ImageChops.lighter(canvas.getchannel("A"), layer.getchannel("A")))
            canvas.paste(merged)
        else:
            canvas.alpha_composite(layer)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sprite")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--anim", help="show every step of this animation (default: first step of each animation)")
    ap.add_argument("--cell", type=int, default=200)
    args = ap.parse_args(argv)
    s = load(args.sprite)
    cells = []
    if args.anim:
        match = [a for a in s["anims"] if a[0] == args.anim]
        if not match:
            sys.exit(f"no animation '{args.anim}'; have: {', '.join(a[0] for a in s['anims'])}")
        _, first, n = match[0]
        cells = [(f"{args.anim} {i}", s["steps"][first + i]) for i in range(n)]
    else:
        cells = [(name, s["steps"][first]) for name, first, n in s["anims"]]
    cols = min(6, len(cells)); rows = (len(cells) + cols - 1) // cols
    C = args.cell
    sheet = Image.new("RGBA", (cols * C, rows * C), (40, 44, 60, 255))
    for i, (label, (frame, dx, dy, delay)) in enumerate(cells):
        cx, cy = (i % cols) * C, (i // cols) * C
        cell = Image.new("RGBA", (C, C), (40 + 12 * ((i + i // cols) % 2), 44, 60, 255))
        draw_frame(s, frame, cell, C * 2 // 3 + dx, C * 4 // 5 + dy)
        sheet.paste(cell, (cx, cy))
    sheet.save(args.out)
    print(f"wrote {args.out}: {len(cells)} frames ({', '.join(c[0] for c in cells[:12])}{' ...' if len(cells) > 12 else ''})")


if __name__ == "__main__":
    main()
