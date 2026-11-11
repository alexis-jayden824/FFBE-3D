#!/usr/bin/env python3
"""Pack master-data tables into a .f3dp blob for the console.

Runs on your PC against data YOU supply. Nothing it reads or writes belongs in
the repository; outputs go to data/ which is git-ignored.

Inputs it understands:
  * TSV tables whose first line is a '# {json}' header with "table" and
    "fields" (the layout used by the tables under assets/rules/content/battle)
  * .ndjson tables (one JSON object per line)
  * an .apk / .zip containing either of the above (read in place, not unpacked)

Column names in the source are opaque keys. tools/assets/schema.json maps
them to readable names as they get identified; unmapped keys pass through
unchanged, so the pack is always complete even while the schema is not.

    python3 tools/assets/mstpack.py path/to/game.apk -o data/master.f3dp
"""
import argparse, io, json, os, re, struct, sys, zipfile

INT_RE = re.compile(r"-?\d{1,10}\Z")

# Console packs: just the tables (and, with --mapped-only, columns) one feature needs.
PRESETS = {
    "encounter": ["F_MISSION_PHASE_MST", "F_BATTLE_LOTTERY_MST", "F_BATTLE_GROUP_MST", "F_MONSTER_PARTS_MST",
                  "F_MONSTER_SKILL_SET_MST", "F_MONSTER_SKILL_MST"],
}


def as_i32(v):
    """Return int if v is a plain integer that fits in i32, else None."""
    if isinstance(v, bool):
        return None
    if isinstance(v, int):
        return v if -2**31 <= v < 2**31 else None
    if isinstance(v, str) and INT_RE.match(v):
        n = int(v)
        # keep things like "007" as text so ids round-trip exactly
        return n if -2**31 <= n < 2**31 and str(n) == v else None
    return None


def parse_tsv(name, text):
    lines = text.split("\n")
    if not lines or not lines[0].startswith("# "):
        raise ValueError(f"{name}: missing '# {{json}}' header line")
    head = json.loads(lines[0][2:])
    fields = head["fields"]
    rows = []
    for ln, line in enumerate(lines[1:], 2):
        if not line:
            continue
        cells = line.split("\t")
        if len(cells) != len(fields):
            raise ValueError(f"{name}:{ln}: {len(cells)} cells, expected {len(fields)}")
        rows.append(cells)
    return head.get("table") or os.path.splitext(os.path.basename(name))[0], fields, rows


def parse_ndjson(name, text):
    objs = [json.loads(l) for l in text.split("\n") if l.strip()]
    fields = []
    for o in objs:
        for k in o:
            if k not in fields:
                fields.append(k)
    rows = []
    for o in objs:
        row = []
        for k in fields:
            v = o.get(k, "")
            row.append(v if isinstance(v, (str, int)) and not isinstance(v, bool)
                       else json.dumps(v, ensure_ascii=False, separators=(",", ":")))
        rows.append(row)
    return os.path.splitext(os.path.basename(name))[0], fields, rows


def load_sources(paths):
    """Yield (table_name, fields, rows) for every table found."""
    def handle(name, data):
        if name.endswith(".tsv"):
            yield parse_tsv(name, data.decode("utf-8"))
        elif name.endswith(".ndjson"):
            yield parse_ndjson(name, data.decode("utf-8"))
        elif name.endswith((".apk", ".zip")):
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                for inner in sorted(z.namelist()):
                    if inner.endswith((".tsv", ".ndjson", ".zip")):
                        yield from handle(inner, z.read(inner))

    for p in paths:
        if os.path.isdir(p):
            for root, _, files in sorted(os.walk(p)):
                for f in sorted(files):
                    full = os.path.join(root, f)
                    with open(full, "rb") as fh:
                        yield from handle(full, fh.read())
        else:
            with open(p, "rb") as fh:
                yield from handle(p, fh.read())


def build(tables, schema):
    """tables: list of (name, fields, rows). Returns bytes."""
    strings = bytearray()
    index = {}

    def intern(s):
        if s not in index:
            index[s] = len(strings)
            strings.extend(s.encode("utf-8") + b"\0")
        return index[s]

    prepared = []
    for name, fields, rows in tables:
        names = [schema.get(name, {}).get(k, k) for k in fields]
        ncol = len(fields)
        # A column is i32 only if every cell is.
        is_int = [all(as_i32(r[c]) is not None for r in rows) if rows else False for c in range(ncol)]
        if ncol and is_int[0]:
            rows = sorted(rows, key=lambda r: as_i32(r[0]))
        prepared.append((name, names, is_int, rows))

    header_size = 12 + 20 * len(prepared)
    cols_size = sum(8 * len(p[1]) for p in prepared)
    rows_size = sum(4 * len(p[1]) * len(p[3]) for p in prepared)
    str_base = header_size + cols_size + rows_size

    out = bytearray(struct.pack("<4sII", b"F3DP", 1, len(prepared)))
    cols_blob, rows_blob = bytearray(), bytearray()
    for name, names, is_int, rows in prepared:
        out += struct.pack("<IIIII", str_base + intern(name), len(names), len(rows),
                           header_size + len(cols_blob), header_size + cols_size + len(rows_blob))
        for n, i in zip(names, is_int):
            cols_blob += struct.pack("<II", str_base + intern(n), 0 if i else 1)
        for r in rows:
            for c, cell in enumerate(r):
                if is_int[c]:
                    rows_blob += struct.pack("<i", as_i32(cell))
                else:
                    rows_blob += struct.pack("<I", str_base + intern(cell if isinstance(cell, str) else str(cell)))
    if not strings:
        strings.extend(b"\0")
    return bytes(out + cols_blob + rows_blob + strings)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="tsv / ndjson files, folders, or an apk/zip")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--schema", default=os.path.join(os.path.dirname(__file__), "schema.json"))
    ap.add_argument("--only", nargs="*", help="pack only these table names")
    ap.add_argument("--mapped-only", action="store_true",
                    help="drop columns that have no schema mapping (smaller packs for the console)")
    ap.add_argument("--preset", choices=sorted(PRESETS),
                    help="shorthand for a console pack: " + ", ".join(sorted(PRESETS)))
    args = ap.parse_args(argv)

    schema = {}
    if os.path.exists(args.schema):
        with open(args.schema, encoding="utf-8") as f:
            schema = {k: v for k, v in json.load(f).items() if not k.startswith("_")}

    if args.preset:
        args.only = PRESETS[args.preset]
        args.mapped_only = True

    tables, seen = [], set()
    for name, fields, rows in load_sources(args.inputs):
        if args.only and name not in args.only:
            continue
        if name in seen:
            print(f"warning: duplicate table {name} skipped", file=sys.stderr)
            continue
        seen.add(name)
        if args.mapped_only:
            keep = [i for i, k in enumerate(fields) if k in schema.get(name, {})]
            if not keep:
                print(f"warning: {name} has no mapped columns, skipped", file=sys.stderr)
                continue
            fields = [fields[i] for i in keep]
            rows = [[r[i] for i in keep] for r in rows]
        tables.append((name, fields, rows))
    if not tables:
        sys.exit("no tables found")

    blob = build(tables, schema)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(blob)
    for name, fields, rows in tables:
        print(f"{name:32s} {len(rows):7d} rows  {len(fields):3d} cols")
    print(f"wrote {args.out}: {len(blob) / 1e6:.1f} MB, {len(tables)} tables")


if __name__ == "__main__":
    main()
