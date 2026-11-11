#!/usr/bin/env python3
"""Download the game data packs listed in a pack manifest to a local folder.

Runs on your PC. The packs are the same ones the Android app downloads on its
first launch; this just fetches them to a folder of your choice so the
converters can read them. Nothing downloaded here belongs in the repository
(the default folder, data/packs, is git-ignored).

    python tools/assets/fetch_packs.py --dest D:\\FFBE-data
    python tools/assets/fetch_packs.py --dest D:\\FFBE-data --only client-assets data-001
    python tools/assets/fetch_packs.py --dest D:\\FFBE-data --verify

Safe to stop and rerun: finished packs are skipped after a checksum check and
partial downloads resume where they stopped. A pack is only given its final
name once its SHA-256 matches the manifest.
"""
import argparse, hashlib, json, os, shutil, sys, time, urllib.error, urllib.request

DEFAULT_MANIFEST = "https://raw.githubusercontent.com/Elvlin/exvius-offline/main/data/latest.json"
CHUNK = 1 << 20


def load_manifest(src):
    if src.startswith(("http://", "https://")):
        with urllib.request.urlopen(src, timeout=60) as r:
            return json.load(r)
    with open(src, encoding="utf-8") as f:
        return json.load(f)


def sha256_of(path, progress=None):
    h = hashlib.sha256()
    done = 0
    with open(path, "rb") as f:
        while True:
            b = f.read(CHUNK)
            if not b:
                break
            h.update(b)
            done += len(b)
            if progress:
                progress(done)
    return h.hexdigest()


def human(n):
    return f"{n / 1e9:.2f} GB" if n >= 1e9 else f"{n / 1e6:.1f} MB"


def download(pack, dest, part):
    """Fetch pack['url'] into `part`, resuming if it already has bytes."""
    size = pack["size"]
    have = os.path.getsize(part) if os.path.exists(part) else 0
    if have > size:
        os.remove(part)
        have = 0
    last = 0.0
    while have < size:
        req = urllib.request.Request(pack["url"], headers={"Range": f"bytes={have}-"} if have else {})
        try:
            with urllib.request.urlopen(req, timeout=60) as r, open(part, "ab" if have else "wb") as f:
                if have and r.status != 206:      # server ignored the range: start over
                    f.close()
                    os.remove(part)
                    have = 0
                    continue
                while True:
                    b = r.read(CHUNK)
                    if not b:
                        break
                    f.write(b)
                    have += len(b)
                    now = time.time()
                    if now - last > 1:
                        print(f"\r  {pack['id']}: {human(have)} / {human(size)} ({100 * have // size}%)   ", end="", flush=True)
                        last = now
        except (urllib.error.URLError, ConnectionError, TimeoutError) as e:
            print(f"\n  connection problem ({e}); retrying in 5 s from {human(have)}")
            time.sleep(5)
    print(f"\r  {pack['id']}: {human(have)} / {human(size)} (100%)   ")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dest", default=os.path.join("data", "packs"), help="folder to store packs in")
    ap.add_argument("--manifest", default=DEFAULT_MANIFEST, help="pack manifest URL or local file")
    ap.add_argument("--only", nargs="*", help="pack ids to fetch (default: all)")
    ap.add_argument("--list", action="store_true", help="show the packs and exit")
    ap.add_argument("--verify", action="store_true", help="re-check checksums of packs already downloaded; download nothing")
    args = ap.parse_args(argv)

    manifest = load_manifest(args.manifest)
    packs = manifest["packs"]
    if args.only:
        unknown = set(args.only) - {p["id"] for p in packs}
        if unknown:
            sys.exit(f"unknown pack id(s): {', '.join(sorted(unknown))}")
        packs = [p for p in packs if p["id"] in args.only]

    total = sum(p["size"] for p in packs)
    print(f"manifest version {manifest.get('version')}: {len(packs)} packs, {human(total)}")
    if args.list:
        for p in packs:
            print(f"  {p['id']:28s} {human(p['size']):>10s}  {os.path.basename(p['url'])}")
        return 0

    os.makedirs(args.dest, exist_ok=True)
    with open(os.path.join(args.dest, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    failed = []
    for p in packs:
        final = os.path.join(args.dest, os.path.basename(p["url"]))
        part = final + ".part"
        if os.path.exists(final):
            ok = os.path.getsize(final) == p["size"] and sha256_of(final) == p["sha256"]
            print(f"  {p['id']}: {'already downloaded, checksum ok' if ok else 'CHECKSUM MISMATCH'}")
            if ok:
                continue
            if args.verify:
                failed.append(p["id"])
                continue
            os.replace(final, part)      # keep the bytes; a resume may complete it, else it is refetched
        elif args.verify:
            print(f"  {p['id']}: not downloaded")
            continue

        need = p["size"] - (os.path.getsize(part) if os.path.exists(part) else 0)
        free = shutil.disk_usage(args.dest).free
        if free < need + (64 << 20):
            sys.exit(f"not enough free space in {args.dest}: need {human(need)}, have {human(free)}")

        download(p, args.dest, part)
        if sha256_of(part) != p["sha256"]:
            os.remove(part)
            print(f"  {p['id']}: checksum mismatch after download; removed, rerun to fetch it again")
            failed.append(p["id"])
            continue
        os.replace(part, final)
        print(f"  {p['id']}: verified")

    if failed:
        print(f"FAILED: {', '.join(failed)}")
        return 1
    print("verify finished" if args.verify else f"all requested packs are in {os.path.abspath(args.dest)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
