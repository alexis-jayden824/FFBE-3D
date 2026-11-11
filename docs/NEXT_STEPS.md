# Next steps

State as of 2026-11-11. Milestone 0 audit is written; Milestone 1 is started; the first build boots on the owner's New 3DS XL.

## Blocked on the owner

1. **Rebuild the pack and try the new build**: rerun `mstpack.py --preset encounter` (now 6 tables, about 4.0 MB), recopy it, and report whether enemy skill names appear on the top screen after the enemy turn, plus the still-unanswered items: do Japanese monster names render, and what frame time does the Select overlay show on a large wave.
2. **Decide how decoded unit/ability tables are obtained.** They are in encrypted `.dat` files. Options: (a) run upstream's own `source/exvius/decode_client_data.py` locally and point our converter at its output; (b) defer units and continue with the monster/encounter/equipment tables already readable.
3. **Download the data packs to your PC** with `tools/assets/fetch_packs.py --dest <folder>` (14.65 GB; pick a folder outside OneDrive and outside the repo). This is the durable copy; Claude's workspace copy is temporary.

## Milestone 1 - platform foundation (in progress)

- [x] Dual-screen battle shell with buttons and touch (compiles in CI; not yet run)
- [x] CI: host tests + 3dsx build job with artifact upload
- [x] First green console build (run 43006702872)
- [ ] Pin the toolchain image digest
- [x] SD read of a `.f3dp` pack with a clear message when missing or corrupt (compiled; not yet run on hardware)
- [ ] Audio output (one streamed track, one sound effect) via ndsp
- [x] Diagnostics overlay: frame time, worst frame, free linear memory (not yet run on hardware)
- [ ] Settings file on SD
- [ ] Sleep/resume and clean exit handling
- [ ] Move scenes out of `main.cpp` into `src/ui/` with a proper scene stack (three scenes live in `main.cpp` today)
- [ ] Separate "edit" and "activate" battle states; confirm dialogs; input-bleed guard

## Milestone 2 - real vertical slice

- [x] CPK archive reader (`tools/assets/cpk.py`); UI art formats identified (PNG, plist, ssbp, fnt, layout CSV)
- [x] Pack downloader with resume and checksums (`tools/assets/fetch_packs.py`)
- [ ] Measure PNG sizes against the 1024x1024 texture limit; choose scale and atlas rules
- [ ] Convert one battle background and the battle UI art to console textures; draw them
- [ ] Open a large data pack; identify unit sprite and map formats
- [ ] Convert one unit's sprite and animation data to a citro2d atlas
- [x] Map the encounter chain columns in `schema.json` (stat order still inferred)
- [x] Resolve a real mission into waves and spawn them into `Battle` (host only; `f3d_inspect`)
- [ ] Verify ATK/DEF/MAG/SPR column order against an observed fight
- [ ] Multi-part monsters: only the body part is spawned today
- [x] Enemy damage skills from data: name, elements, attack type, hit timing (host-tested; console compiled)
- [ ] Decode skill power, targeting, cost and effect list (20 undecoded columns)
- [ ] Non-damage enemy skills (27,129 slots: heals, buffs, scripted messages)
- [ ] Enemy AI conditions (skills are chosen at random today)
- [x] Load the pack from SD on console and fight a mission's real waves there (placeholder party and attacks; not yet run on hardware)
- [ ] One exploration area with Circle Pad movement and collision
- [ ] Rewards per upstream `ffbe_battle.py` rules; versioned save with atomic write; reload

## Core work that needs no hardware

- [ ] Mission clear conditions (upstream `_cond_*` list) as tested core rules
- [ ] Drop rolling with seeded statistical tests
- [ ] Save schema draft (`docs/SAVE_FORMAT.md`) after reading upstream `player_state.py`
- [ ] Summon rules: rates, guarantees, step-ups, with deterministic tests
- [x] Small per-feature console packs (`--preset encounter`, 2.6 MB)
- [x] Content validator for the encounter chain (`f3d_inspect validate`)
- [ ] Classify the 863 unresolved missions (inherited upstream gaps vs our bugs)

## Docs still to write

`SETUP.md`, `SAVE_FORMAT.md`, `PERFORMANCE.md` - deliberately not created until there is real content for them.
