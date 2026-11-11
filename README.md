# FFBE 3D

A native New Nintendo 3DS engine for playing an offline, preserved copy of *Final Fantasy Brave Exvius*, using both screens and physical controls.

**Status: early development. Not playable.** The battle core passes its PC tests; the console build has not yet been compiled or run. See [docs/TEST_REPORT.md](docs/TEST_REPORT.md).

This is an unofficial fan project, not affiliated with Square Enix or gumi. **The repository contains no game content** - only original engine code and conversion tools. You supply your own data; converted output stays on your machine.

## What exists

- `src/core/` - deterministic battle core: damage, elements, buffs, guard, timed multi-hit chaining with spark and cap.
- `src/content/` - zero-copy reader for converted table packs.
- `src/platform/3ds/` - dual-screen battle shell (top: field, bottom: six command cards; buttons and touch).
- `tools/assets/mstpack.py` - packs master-data tables from an APK, zip, folder, `.tsv` or `.ndjson`.
- `docs/` - source audit, feature matrix, architecture, risks, next steps.

## Build

Host tests (any PC with CMake, a C++17 compiler, Python 3):

```
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

Console (`.3dsx`, needs devkitPro's `3ds-dev` packages):

```
make
```

Convert tables from data you own:

```
python3 tools/assets/mstpack.py path/to/exvius-offline.apk -o data/master.f3dp
```

## Background

Reference project: [Elvlin/exvius-offline](https://github.com/Elvlin/exvius-offline) (Android). The audit in [docs/SOURCE_AUDIT.md](docs/SOURCE_AUDIT.md) found that it runs the original client binary against a local server, so the 3DS client is a from-scratch reimplementation.
