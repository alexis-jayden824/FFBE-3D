# Project instructions for Claude sessions

Native New 3DS reimplementation of the client side of FFBE offline. Owner's full spec: the "Exvius Offline Native New Nintendo 3DS Project Brief" (2026-11-11); its milestones and rules govern this repo.

## Read first

`docs/NEXT_STEPS.md` (exact next tasks), `docs/SOURCE_AUDIT.md` (what upstream is), `docs/FEATURE_PARITY.md` (status of every feature), `docs/TEST_REPORT.md` (what has actually been run).

## Hard rules

- **Never commit game content**: no APKs, tables, text, art, audio, or converter output. `data/` is git-ignored. Test fixtures are original placeholders.
- **Do not copy upstream code.** Upstream (`Elvlin/exvius-offline`, pinned `b0b73648ceb1a3783105ed2104600be651c9c515`) has no license. Read it for behaviour; write our own.
- **Do not claim something works unless it was run.** Record host / emulator / hardware results separately in `docs/TEST_REPORT.md`; mark anything not run as not run.
- Core stays deterministic: integers and permille, injected seed, no platform headers in `src/core` or `src/content`.
- Rendering never changes mechanics; chain timing uses the 60 Hz simulation clock.
- Add a `tools/assets/schema.json` column mapping only after verifying it against data.
- Update `FEATURE_PARITY.md`, `TEST_REPORT.md` and `NEXT_STEPS.md` in the same commit as the work.

## Commands that have actually worked

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure
python3 tools/assets/mstpack.py <apk|zip|folder|tsv|ndjson> -o data/master.f3dp
./build/f3d_inspect data/master.f3dp validate      # or: tables | mission <id>
```

`make` (console build) passes in CI (`devkitpro/devkitarm:latest`); the first build boots on the owner's New 3DS XL (inputs not yet confirmed).

## Key facts

- Upstream = original `libgame.so` (arm64, Cocos) + Frida hooks + local Python server. Client logic has no source; server-side rules (rewards, drops, gacha, shops, clear conditions) are in `source/server/*.py`.
- APK contains tables and text only; sprites/maps/audio are in ~14.65 GB of CPK/ACB release packs, formats unverified.
- Battle formulas in `src/core/battle.cpp` are from general knowledge and unverified against the real game.
- Encounter chain (mission -> phases -> lottery -> group -> monster parts) is implemented in `src/content/encounter.cpp`; battle phases have `phase_type` 2. HP needs 64 bits. ATK/DEF/MAG/SPR column order is inferred, not verified.
- Console reads `sdmc:/3ds/ffbe3d/encounter.f3dp` (built with `mstpack.py --preset encounter`, 2.6 MB). Scenes (select/battle/result) currently live in `src/platform/3ds/main.cpp`. Party and attacks are placeholders.
- Enemy skills: `src/content/skills.cpp` loads damage skills (name, element flags, attack type, hit frames - all verified by correlation with skill names, see docs/ASSETS.md). Power/targeting/effects/AI are undecoded; do not guess them.
- Data packs: fetch with `tools/assets/fetch_packs.py` (manifest-driven, resumable, SHA-256). Read archives with `tools/assets/cpk.py`. UI art is PNG + Cocos plist + SpriteStudio ssbp + .fnt + layout CSV inside CPK. Never unpack or copy pack contents into the repo.
