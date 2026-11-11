# FFBE 3D

A native New Nintendo 3DS engine for playing an offline, preserved copy of *Final Fantasy Brave Exvius*, using both screens and physical controls.

**Status: early development. Not playable.** The battle core passes its PC tests; the console build boots on a New 3DS XL and shows a placeholder battle screen. See [docs/TEST_REPORT.md](docs/TEST_REPORT.md).

This is an unofficial fan project, not affiliated with Square Enix or gumi. **The repository contains no game content** - only original engine code and conversion tools. You supply your own data; converted output stays on your machine.

## What exists

- `src/core/` - deterministic battle core: damage, elements, buffs, guard, timed multi-hit chaining with spark and cap.
- `src/content/` - zero-copy reader for converted table packs; resolves a mission into its enemy waves.
- `src/platform/3ds/` - mission select, multi-wave battle and result scenes across both screens; loads real enemy waves from an SD pack if present.
- `tools/inspect/` - PC tool to list tables, print a mission's waves, and validate the encounter chain.
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

Convert tables from data you own (see [docs/SETUP.md](docs/SETUP.md) for the SD card layout):

```
python3 tools/assets/mstpack.py path/to/exvius-offline.apk --preset encounter -o encounter.f3dp
```

## Background

Reference project: [Elvlin/exvius-offline](https://github.com/Elvlin/exvius-offline) (Android). The audit in [docs/SOURCE_AUDIT.md](docs/SOURCE_AUDIT.md) found that it runs the original client binary against a local server, so the 3DS client is a from-scratch reimplementation.
