#!/usr/bin/env python3
"""Read CRI CPK archives (the container the game's art, maps and scenes ship in).

Runs on your PC against packs YOU supply; nothing it reads or writes belongs
in the repository.

    python3 tools/assets/cpk.py list  path/to/file.cpk
    python3 tools/assets/cpk.py stats path/to/file.cpk
    python3 tools/assets/cpk.py extract path/to/file.cpk -o data/raw/

Supports plain and obfuscated @UTF tables, TOC-based archives, and CRILAYLA
compressed entries. ITOC-only (id-indexed) archives are reported, not read.
"""
import argparse, os, struct, sys
from collections import Counter

# @UTF column storage / type flags
STORAGE_ZERO, STORAGE_CONST, STORAGE_ROW = 0x10, 0x30, 0x50
TYPES = {0: ">B", 1: ">b", 2: ">H", 3: ">h", 4: ">I", 5: ">i", 6: ">Q", 7: ">q", 8: ">f", 9: ">d"}


def deobfuscate(buf):
    out = bytearray(len(buf)); m = 0x655F
    for i, b in enumerate(buf):
        out[i] = b ^ (m & 0xFF); m = (m * 0x4115) & 0xFFFFFFFF
    return bytes(out)


def read_utf(buf):
    """Parse an @UTF table. Returns list of row dicts."""
    if buf[:4] != b"@UTF":
        buf = deobfuscate(buf)
        if buf[:4] != b"@UTF":
            raise ValueError("not an @UTF table")
    size, = struct.unpack_from(">I", buf, 4)
    body = buf[8:8 + size]
    rows_off, str_off, data_off, name_off, ncol, row_len, nrow = struct.unpack_from(">HIIIHHI", body, 0)
    # the first field is 2 bytes in v1 tables: re-read with the standard layout
    _, rows_off, str_off, data_off, name_off, ncol, row_len, nrow = struct.unpack_from(">HHIIIHHI", body, 0)

    def cstr(off):
        s = str_off + off
        return body[s:body.index(b"\0", s)].decode("utf-8", "replace")

    def value(typ, pos):
        if typ in TYPES:
            v, = struct.unpack_from(TYPES[typ], body, pos)
            return v, struct.calcsize(TYPES[typ])
        if typ == 0xA:
            o, = struct.unpack_from(">I", body, pos)
            return cstr(o), 4
        if typ == 0xB:
            o, n = struct.unpack_from(">II", body, pos)
            return body[data_off + o:data_off + o + n], 8
        raise ValueError(f"unknown @UTF type {typ}")

    cols, pos = [], 24
    for _ in range(ncol):
        flags = body[pos]; pos += 1
        name_o, = struct.unpack_from(">I", body, pos); pos += 4
        storage, typ = flags & 0xF0, flags & 0x0F
        const = None
        if storage == STORAGE_CONST:
            const, n = value(typ, pos); pos += n
        cols.append((cstr(name_o), storage, typ, const))

    rows = []
    for r in range(nrow):
        pos = rows_off + r * row_len
        row = {}
        for name, storage, typ, const in cols:
            if storage == STORAGE_ROW:
                row[name], n = value(typ, pos); pos += n
            elif storage == STORAGE_CONST:
                row[name] = const
            else:
                row[name] = 0 if typ < 0xA else ""
        rows.append(row)
    return rows


def crilayla(buf):
    """Decompress a CRILAYLA block."""
    usize, csize = struct.unpack_from("<II", buf, 8)
    out = bytearray(usize + 0x100)
    out[:0x100] = buf[16 + csize:16 + csize + 0x100]
    src = buf
    in_pos = 16 + csize - 1          # read backwards from the end of the compressed stream
    bit_pool, bits_left = 0, 0

    def bits(n):
        nonlocal in_pos, bit_pool, bits_left
        v = 0
        while n:
            if not bits_left:
                bit_pool = src[in_pos]; in_pos -= 1; bits_left = 8
            take = min(n, bits_left)
            v = (v << take) | ((bit_pool >> (bits_left - take)) & ((1 << take) - 1))
            bits_left -= take; n -= take
        return v

    end = 0x100 + usize - 1
    done = 0
    levels = (2, 3, 5, 8)
    while done < usize:
        if bits(1):
            off = end - done + bits(13) + 3
            length = 3
            for lv in levels:
                t = bits(lv); length += t
                if t != (1 << lv) - 1:
                    break
            else:
                while True:
                    t = bits(8); length += t
                    if t != 255:
                        break
            for _ in range(length):
                if done >= usize:
                    break
                out[end - done] = out[off]; off -= 1; done += 1
        else:
            out[end - done] = bits(8); done += 1
    return bytes(out)


class Cpk:
    def __init__(self, path):
        """`path` is a file name, or an open seekable binary file (e.g. io.BytesIO)."""
        self.f = open(path, "rb") if isinstance(path, (str, bytes, os.PathLike)) else path
        if self.f.read(4) != b"CPK ":
            raise ValueError(f"{path}: not a CPK archive")
        self.header = self._table(0)[0]
        self.entries = []
        toc = self.header.get("TocOffset") or 0
        self.itoc_only = not toc and bool(self.header.get("ItocOffset"))
        if toc:
            content = self.header.get("ContentOffset") or 0
            base = min(toc, content) if content else toc
            for r in self._table(toc):
                name = (r.get("DirName") + "/" if r.get("DirName") else "") + r.get("FileName", "")
                self.entries.append((name, base + r["FileOffset"], r["FileSize"], r.get("ExtractSize", r["FileSize"])))

    def _table(self, off):
        self.f.seek(off + 4)
        _, size = struct.unpack("<IQ", self.f.read(12))
        return read_utf(self.f.read(size))

    def read(self, entry):
        _, off, size, _ = entry
        self.f.seek(off)
        raw = self.f.read(size)
        return crilayla(raw) if raw[:8] == b"CRILAYLA" else raw


def sniff(data):
    for magic, kind in ((b"\x89PNG", "png"), (b"PVR\x03", "pvr3"), (b"PKM ", "pkm"), (b"CCZ!", "ccz"), (b"\x1f\x8b", "gzip"),
                        (b"<?xml", "xml"), (b"@UTF", "utf"), (b"AFS2", "awb"), (b"HCA\0", "hca"), (b"OggS", "ogg"), (b"{", "json"), (b"[", "json?")):
        if data.startswith(magic):
            return kind
    try:
        data[:64].decode("utf-8"); return "text"
    except UnicodeDecodeError:
        return "binary"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["list", "stats", "extract"])
    ap.add_argument("cpk")
    ap.add_argument("-o", "--out", default="data/raw")
    args = ap.parse_args(argv)
    c = Cpk(args.cpk)
    if c.itoc_only:
        sys.exit(f"{args.cpk}: id-indexed (ITOC-only) archive; not supported yet")
    if args.cmd == "list":
        for e in c.entries:
            print(f"{e[2]:10d} {e[3]:10d} {e[0]}")
    elif args.cmd == "stats":
        ext, kinds, packed, unpacked = Counter(), Counter(), 0, 0
        for e in c.entries:
            data = c.read(e)
            ext[os.path.splitext(e[0])[1].lower() or "(none)"] += 1
            kinds[sniff(data)] += 1
            packed += e[2]; unpacked += len(data)
        print(f"{len(c.entries)} files, {packed / 1e6:.1f} MB packed, {unpacked / 1e6:.1f} MB unpacked")
        print("by extension:", dict(ext.most_common()))
        print("by content:  ", dict(kinds.most_common()))
    else:
        root = os.path.abspath(args.out)
        for e in c.entries:
            dest = os.path.abspath(os.path.join(root, e[0]))
            if not dest.startswith(root + os.sep):
                sys.exit(f"refusing to write outside the output folder: {e[0]}")
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "wb") as f:
                f.write(c.read(e))
        print(f"extracted {len(c.entries)} files to {root}")


if __name__ == "__main__":
    main()
