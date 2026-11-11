#!/usr/bin/env python3
"""Build everything the console needs for a party, laid out like the SD card.

Runs on your PC against the pack folder made by fetch_packs.py. Output is
derived from game data: keep it out of the repository. Needs Pillow.

    python tools/assets/party.py --packs C:\\FFBE-data --units 100000107 100000207 100000305
    python tools/assets/unitsprite.py --packs C:\\FFBE-data --find lasswell      (to look up ids)

Writes, under the output folder (default: sd_out):

    3ds/ffbe3d/party.txt            the unit ids, one per line, in party order (up to six)
    3ds/ffbe3d/units.f3dp           unit stats and names for every unit in the packs
    3ds/ffbe3d/units/<id>.f3ds      one sprite per unit

Copy the contents of the output folder onto the SD card (merge with the
existing /3ds folder).
"""
import argparse, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mstpack, unitsprite


def is_current(path):
    try:
        with open(path, "rb") as f:
            magic, ver = struct.unpack("<4sI", f.read(8))
        return magic == b"F3DS" and ver == unitsprite.FILE_VERSION
    except (OSError, struct.error):
        return False


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--packs", required=True, help="folder made by fetch_packs.py")
    ap.add_argument("--units", nargs="+", required=True, help="one to six unit ids, in party order")
    ap.add_argument("-o", "--out", default="sd_out")
    ap.add_argument("--max-atlas", type=int, default=1024, choices=[256, 512, 1024], help="largest sprite texture side (default 1024)")
    ap.add_argument("--force", action="store_true", help="convert sprites again even if they already exist")
    args = ap.parse_args(argv)
    if len(args.units) > 6:
        sys.exit("a party has at most six units")
    for u in args.units:
        if not u.isdigit():
            sys.exit(f"'{u}' is not a unit id (ids are numbers; find them with unitsprite.py --find <name>)")

    base = os.path.join(args.out, "3ds", "ffbe3d")
    os.makedirs(os.path.join(base, "units"), exist_ok=True)

    for u in args.units:
        dest = os.path.join(base, "units", f"{u}.f3ds")
        if not args.force and is_current(dest):
            print(f"unit {u}: sprite already converted")
            continue
        files = unitsprite.load_from_packs(args.packs, u)
        blob, info, _ = unitsprite.convert(files, u, 1.0, None, args.max_atlas)
        with open(dest, "wb") as f:
            f.write(blob)
        print(f"unit {u}: sprite {info['atlas'][0]}x{info['atlas'][1]} at scale {info['scale']}, {len(blob) / 1e6:.1f} MB")

    mstpack.main([args.packs, "--preset", "units", "-o", os.path.join(base, "units.f3dp")])

    with open(os.path.join(base, "party.txt"), "w", newline="\n") as f:
        f.write("\n".join(args.units) + "\n")
    print(f"party: {', '.join(args.units)}")
    print(f"done. Copy the contents of '{os.path.abspath(args.out)}' onto the SD card.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
