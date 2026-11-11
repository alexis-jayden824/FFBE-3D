# Next steps

State as of 2026-11-11. Milestone 0 audit is written; Milestone 1 is started; the first build boots on the owner's New 3DS XL.

## Blocked on the owner

1. **Build a real party and try the new build** (`docs/SETUP.md` section 3): run `party.py` with up to six unit ids, copy `sd_out` to the SD card, install the new `.3dsx`. Report whether names and HP on the bottom screen are the real ones, and the frame time and `linear free` from the Select overlay.
2. Data packs: **done** - the owner has all 19 packs verified in `C:\FFBE-data`.

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
- [x] Unit sprite format identified (sheet PNG + cgg frame CSV + cgs animation CSVs)
- [x] Unit sprite converter and PC preview (`unitsprite.py`, `spritepreview.py`); field meanings verified by eye
- [x] Console: load `.f3ds`, draw animated party sprites (compiled; awaiting hardware run)
- [x] Confirmed on hardware: orientation, animation playback, six sprites loaded
- [ ] Confirm on hardware: rotation direction of effect pieces, additive effects, frame time, memory headroom
- [ ] Decide per ability whether the unit runs up or attacks in place (needs ability data)
- [ ] Enemy sprites (monster art format not yet inspected)
- [x] Party units from data: max-level stats, English names, normal-attack timing (`party.py`, `content/units`); compiled, awaiting hardware run
- [ ] Unit abilities, magic and limit bursts (F_ABILITY_MST, F_MAGIC_MST, F_LIMITBURST_MST and the unit's learn list)
- [ ] Level growth below max level; equipment; passives
- [ ] Open map and battle background archives
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
