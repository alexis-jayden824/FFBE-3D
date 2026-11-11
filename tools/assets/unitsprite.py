#!/usr/bin/env python3
"""Convert one unit's battle sprite into a console sprite file (.f3ds).

Runs on your PC against data YOU supply (the pack folder made by
fetch_packs.py). Output is derived from game art: keep it out of the repo.
Needs Pillow:  python -m pip install pillow

    python tools/assets/unitsprite.py --packs C:\\FFBE-data --find rain
    python tools/assets/unitsprite.py --packs C:\\FFBE-data --unit 100000102 -o party/1.f3ds
    python tools/assets/unitsprite.py --dir some/folder --unit 900000001 -o test.f3ds

Source format (per unit id):
    unit_anime_<id>.png       sprite sheet
    unit_cgg_<id>.csv         one line per frame: anchor, part count, then 11
                              numbers per part: x, y, flip, blend, opacity,
                              rotation, sheet x, sheet y, width, height, page
    unit_<anim>_cgs_<id>.csv  one line per step: frame, x, y, delay (in 1/60 s)

What the converter does: keeps only the sheet rectangles the frames use,
keeps them at original size where they fit (shrinking them all by one factor
if not), packs them into a power-of-two atlas no larger than 1024x1024 (the
console's texture limit), and writes the atlas in the
console's native texture layout together with the frame and animation tables.

File layout is documented in src/content/sprite.h.
"""
import argparse, glob, io, json, os, re, struct, sys, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cpk as cpklib

MAX_ATLAS = 1024
FILE_VERSION = 2          # 2: texture stored top row first (version 1 was bottom-first and drew garbled on hardware)
ANIM_ORDER = ["idle", "standby", "atk", "magic_standby", "magic_atk", "limit_atk", "move", "jump",
              "dying", "dead", "win_before", "win"]
# Converted by default. Limit burst animations carry large effect art that can
# quadruple the texture, so they are opt-in (--anims all, or name them).
DEFAULT_ANIMS = ["idle", "standby", "atk", "magic_standby", "magic_atk", "move", "jump", "dying", "dead", "win_before", "win"]


def need_pillow():
    try:
        from PIL import Image
        return Image
    except ImportError:
        sys.exit("This tool needs Pillow. Install it with:  python -m pip install pillow")


# ---- locating a unit's files --------------------------------------------------
def unit_archives(packs):
    """Yield (zip path, member name) for every unit sprite archive in the pack folder."""
    for zp in sorted(glob.glob(os.path.join(packs, "*.zip"))):
        with zipfile.ZipFile(zp) as z:
            for name in z.namelist():
                if "/sd/unit/" in name and re.match(r"Ver\d+_unit\d*(_common)?\.cpk$", os.path.basename(name)):
                    yield zp, name


def build_index(packs):
    """unit id -> [zip, member]. Cached in <packs>/unit_index.json."""
    cache = os.path.join(packs, "unit_index.json")
    if os.path.exists(cache):
        with open(cache, encoding="utf-8") as f:
            return json.load(f)
    index = {}
    for zp, member in unit_archives(packs):
        print(f"indexing {os.path.basename(zp)}: {os.path.basename(member)}", file=sys.stderr)
        with zipfile.ZipFile(zp) as z:
            arc = cpklib.Cpk(io.BytesIO(z.read(member)))
        for e in arc.entries:
            m = re.match(r"unit_cgg_(\d+)\.csv$", e[0])
            if m:
                index[m.group(1)] = [os.path.basename(zp), member]
    with open(cache, "w", encoding="utf-8") as f:
        json.dump(index, f)
    return index


def load_from_packs(packs, unit):
    index = build_index(packs)
    if unit not in index:
        sys.exit(f"unit {unit} has no sprite in {packs} ({len(index)} units indexed). Try --find <name>.")
    zp, member = index[unit]
    with zipfile.ZipFile(os.path.join(packs, zp)) as z:
        arc = cpklib.Cpk(io.BytesIO(z.read(member)))
    return {e[0]: arc.read(e) for e in arc.entries if e[0].endswith(f"_{unit}.png") or e[0].endswith(f"_{unit}.csv")}


def load_from_dir(folder, unit):
    files = {}
    for p in glob.glob(os.path.join(folder, f"unit_*_{unit}.*")):
        with open(p, "rb") as f:
            files[os.path.basename(p)] = f.read()
    if not files:
        sys.exit(f"no unit_*_{unit}.* files in {folder}")
    return files


def find_units(packs, text):
    """Print unit ids whose English name contains `text` and that have a sprite."""
    index = build_index(packs)
    hits, seen = 0, set()
    for zp in sorted(glob.glob(os.path.join(packs, "*.zip"))):
        with zipfile.ZipFile(zp) as z:
            for name in z.namelist():
                if name.endswith("F_TEXT_UNITS_NAME.ndjson"):
                    for line in z.read(name).decode("utf-8").split("\n"):
                        if not line.strip():
                            continue
                        row = json.loads(line)
                        m = re.search(r"(\d+)$", str(row.get("key", "")))
                        v = row.get("v")
                        label = v[0] if isinstance(v, list) and v else str(v)
                        if m and text.lower() in str(label).lower():
                            base = m.group(1)
                            ids = sorted(u for u in index if u.startswith(base[:-1]) and len(u) == len(base)) or \
                                  ([base] if base in index else [])
                            line_out = f"{label}: {', '.join(ids)}"
                            if ids and line_out not in seen:
                                seen.add(line_out)
                                print(line_out)
                                hits += 1
                    if not hits:
                        print("no match with a sprite")
                    return
    sys.exit("F_TEXT_UNITS_NAME table not found in the pack folder")


# ---- parsing --------------------------------------------------------------------
def parse_cgg(data):
    """Returns list of frames; each frame is a list of part dicts in file order."""
    frames = []
    for line in data.decode("utf-8", "replace").replace("\r", "").split("\n"):
        v = [x for x in line.split(",") if x != ""]
        parts = []
        if len(v) >= 2:
            n = int(v[1])
            if n and (len(v) - 2) >= n * 11:
                for i in range(n):
                    f = [int(float(x)) for x in v[2 + i * 11: 2 + i * 11 + 11]]
                    parts.append(dict(x=f[0], y=f[1], flip=f[2], blend=f[3], opacity=f[4], rot=f[5],
                                      sx=f[6], sy=f[7], w=f[8], h=f[9]))
        elif not line.strip():
            continue
        frames.append(parts)
    return frames


def parse_cgs(data):
    steps = []
    for line in data.decode("utf-8", "replace").replace("\r", "").split("\n"):
        v = [x for x in line.split(",") if x != ""]
        if len(v) >= 4:
            steps.append(tuple(int(float(x)) for x in v[:4]))
    return steps


# ---- atlas ----------------------------------------------------------------------
def pack(sizes, max_atlas=MAX_ATLAS):
    """Shelf-pack (w, h) boxes with 1px padding into the smallest power-of-two atlas
    up to max_atlas. Returns (W, H, positions) or None if they cannot fit."""
    order = sorted(range(len(sizes)), key=lambda i: -sizes[i][1])
    dims = [(w, h) for h in (64, 128, 256, 512, 1024) for w in (64, 128, 256, 512, 1024) if w >= h and w <= 2 * h or w == h]
    for W, H in sorted({d for d in dims if d[0] <= max_atlas}, key=lambda d: (d[0] * d[1], d[0])):
        pos = [None] * len(sizes)
        x = y = shelf = 0
        ok = True
        for i in order:
            w, h = sizes[i]
            if w + 1 > W:
                ok = False
                break
            if x + w + 1 > W:
                x, y, shelf = 0, y + shelf + 1, 0
            if y + h + 1 > H:
                ok = False
                break
            pos[i] = (x, y)
            x += w + 1
            shelf = max(shelf, h)
        if ok:
            return W, H, pos
    return None


def swizzle(img):
    """RGBA image -> the console's native texture bytes: top row first, 8x8 tiles
    in Z-order, each pixel A,B,G,R. The GPU reads memory row 0 as the TOP of the
    texture (v = 1), which is why the console draws a piece at image row y with
    v = 1 - y / height. Confirmed on hardware: a bottom-first texture (file
    version 1) drew the wrong, upside-down pieces."""
    W, H = img.size
    src = img.tobytes()
    out = bytearray(W * H * 4)
    morton = [0] * 64
    for ty in range(8):
        for tx in range(8):
            m = 0
            for b in range(3):
                m |= ((tx >> b) & 1) << (2 * b) | ((ty >> b) & 1) << (2 * b + 1)
            morton[ty * 8 + tx] = m
    for y in range(H):
        row = y * W * 4
        tile_row = (y >> 3) * (W >> 3)
        for x in range(W):
            s = row + x * 4
            d = ((tile_row + (x >> 3)) * 64 + morton[(y & 7) * 8 + (x & 7)]) * 4
            out[d] = src[s + 3]; out[d + 1] = src[s + 2]; out[d + 2] = src[s + 1]; out[d + 3] = src[s]
    return bytes(out)


# ---- conversion -----------------------------------------------------------------
def convert(files, unit, scale, wanted=None, max_atlas=MAX_ATLAS):
    """wanted: animation names to keep (None = DEFAULT_ANIMS, ["all"] = everything)."""
    Image = need_pillow()
    wanted = DEFAULT_ANIMS if wanted is None else wanted
    sheet_name, cgg_name = f"unit_anime_{unit}.png", f"unit_cgg_{unit}.csv"
    if sheet_name not in files or cgg_name not in files:
        sys.exit(f"unit {unit}: missing {sheet_name} or {cgg_name}")
    sheet = Image.open(io.BytesIO(files[sheet_name])).convert("RGBA")
    frames = parse_cgg(files[cgg_name])

    anims = []
    for name in sorted(files):
        m = re.match(rf"unit_(.+)_cgs_{unit}\.csv$", name)
        if m and ("all" in wanted or m.group(1) in wanted):
            steps = [s for s in parse_cgs(files[name]) if 0 <= s[0] < len(frames)]
            if steps:
                anims.append((m.group(1), steps))
    anims.sort(key=lambda a: (ANIM_ORDER.index(a[0]) if a[0] in ANIM_ORDER else len(ANIM_ORDER), a[0]))
    if not anims:
        sys.exit(f"unit {unit}: no animations found")

    # Only frames some animation uses; only rectangles those frames use.
    used = sorted({s[0] for _, steps in anims for s in steps})
    remap = {f: i for i, f in enumerate(used)}
    rect_ids, rects, dropped = {}, [], 0
    out_frames = []
    for f in used:
        parts = []
        for p in reversed(frames[f]):                      # file lists top-most first; store in draw order
            box = (p["sx"], p["sy"], p["sx"] + p["w"], p["sy"] + p["h"])
            if p["w"] <= 0 or p["h"] <= 0 or box[0] < 0 or box[1] < 0 or box[2] > sheet.width or box[3] > sheet.height:
                dropped += 1
                continue
            if box not in rect_ids:
                rect_ids[box] = len(rects)
                rects.append(box)
            parts.append((rect_ids[box], p))
        out_frames.append(parts)

    while True:
        crops = []
        for box in rects:
            c = sheet.crop(box)
            w, h = max(1, round(c.width * scale)), max(1, round(c.height * scale))
            # Resize with premultiplied alpha so transparent pixels do not darken the edges.
            crops.append(c if (w, h) == c.size else c.convert("RGBa").resize((w, h), Image.BOX).convert("RGBA"))
        packed = pack([c.size for c in crops], max_atlas)
        if packed:
            break
        scale *= 0.85                                    # too much art for one atlas: shrink and retry
        if scale < 0.1:
            sys.exit(f"unit {unit}: cannot fit the sprite into a {max_atlas}x{max_atlas} atlas")
    W, H, pos = packed
    atlas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for c, (x, y) in zip(crops, pos):
        atlas.paste(c, (x, y))

    def s16(v):
        return max(-32768, min(32767, int(round(v))))

    blob_rects = b"".join(struct.pack("<4H", x, y, c.width, c.height) for c, (x, y) in zip(crops, pos))
    blob_frames, blob_parts, npart = b"", b"", 0
    stats = dict(rotated=0, additive=0, flipped=0)
    for parts in out_frames:
        blob_frames += struct.pack("<IHH", npart, len(parts), 0)
        for rid, p in parts:
            rot = p["rot"] % 360
            stats["rotated"] += rot != 0
            stats["additive"] += p["blend"] == 1
            stats["flipped"] += p["flip"] != 0
            blob_parts += struct.pack("<Hhhh4B", rid, s16(p["x"] * scale), s16(p["y"] * scale), rot,
                                      p["flip"] & 3, 1 if p["blend"] == 1 else 0,
                                      max(0, min(255, p["opacity"] * 255 // 100)), 0)
            npart += 1
    blob_anims, blob_steps, nstep = b"", b"", 0
    for name, steps in anims:
        blob_anims += struct.pack("<16sIHH", name.encode("ascii", "replace")[:15], nstep, len(steps), 0)
        for f, x, y, delay in steps:
            blob_steps += struct.pack("<HhhH", remap[f], s16(x * scale), s16(y * scale), max(1, min(65535, delay)))
            nstep += 1

    tex = swizzle(atlas)
    header_size = 36
    tex_off = header_size + len(blob_rects) + len(blob_frames) + len(blob_parts) + len(blob_anims) + len(blob_steps)
    header = struct.pack("<4sIHHHHIHHIII", b"F3DS", FILE_VERSION, W, H, len(rects), len(out_frames), npart,
                         len(anims), int(round(scale * 1000)), nstep, tex_off, len(tex))
    assert len(header) == header_size
    info = dict(unit=unit, atlas=(W, H), scale=round(scale, 3), rects=len(rects), frames=len(out_frames), parts=npart,
                anims=[a[0] for a in anims], steps=nstep, dropped_parts=dropped, sheet=sheet.size, **stats)
    return header + blob_rects + blob_frames + blob_parts + blob_anims + blob_steps + tex, info, atlas


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--packs", help="folder made by fetch_packs.py")
    ap.add_argument("--dir", help="folder holding loose unit_* files (instead of --packs)")
    ap.add_argument("--unit", help="unit id, e.g. 100000102")
    ap.add_argument("--find", help="list unit ids whose English name contains this text")
    ap.add_argument("--scale", type=float, default=1.0,
                    help="texture detail relative to the original art (default 1.0; shrinks automatically if the art does not fit)")
    ap.add_argument("--anims", nargs="*", help="animations to keep, or 'all' (default: " + ", ".join(DEFAULT_ANIMS) + ")")
    ap.add_argument("--max-atlas", type=int, default=MAX_ATLAS, choices=[256, 512, 1024], help="largest texture side (default 1024)")
    ap.add_argument("-o", "--out")
    args = ap.parse_args(argv)

    if args.find:
        if not args.packs:
            sys.exit("--find needs --packs")
        find_units(args.packs, args.find)
        return 0
    if not args.unit or not args.out or not (args.packs or args.dir):
        ap.error("need --unit, -o and one of --packs / --dir")
    files = load_from_dir(args.dir, args.unit) if args.dir else load_from_packs(args.packs, args.unit)
    blob, info, _ = convert(files, args.unit, args.scale, args.anims, args.max_atlas)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(blob)
    print(f"unit {info['unit']}: sheet {info['sheet'][0]}x{info['sheet'][1]} -> atlas {info['atlas'][0]}x{info['atlas'][1]} "
          f"at scale {info['scale']}, {info['rects']} pieces, {info['frames']} frames, {len(info['anims'])} animations")
    print(f"  animations: {', '.join(info['anims'])}")
    print(f"  parts: {info['parts']} ({info['rotated']} rotated, {info['flipped']} flipped, {info['additive']} additive, "
          f"{info['dropped_parts']} dropped as out of bounds)")
    print(f"wrote {args.out}: {len(blob) / 1e6:.2f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
