# Next steps

State as of 2026-11-11. Milestone 0 audit is written; Milestone 1 is started; the first build boots on the owner's New 3DS XL.

## Blocked on the owner

1. **Report how the build behaves on the New 3DS XL beyond booting**: do D-pad, L/R, A, X, Y, B, touch and Start work; does the chain counter rise when units are fired quickly; does it exit cleanly; does closing the lid and reopening work.
2. **Decide how decoded unit/ability tables are obtained.** They are in encrypted `.dat` files. Options: (a) run upstream's own `source/exvius/decode_client_data.py` locally and point our converter at its output; (b) defer units and continue with the monster/encounter/equipment tables already readable.
3. **Provide one or two real data packs** (one unit sprite pack, one map pack) locally so asset formats can be identified. Do not commit them.

## Milestone 1 - platform foundation (in progress)

- [x] Dual-screen battle shell with buttons and touch (compiles in CI; not yet run)
- [x] CI: host tests + 3dsx build job with artifact upload
- [x] First green console build (run 43006702872)
- [ ] Pin the toolchain image digest
- [ ] SD read of a `.f3dp` pack with a clear error screen when missing or corrupt
- [ ] Audio output (one streamed track, one sound effect) via ndsp
- [ ] Settings file on SD; diagnostics overlay (frame time, heap, linear memory)
- [ ] Sleep/resume and clean exit handling
- [ ] Scene stack + focus navigation in `src/ui/`
- [ ] Separate "edit" and "activate" battle states; confirm dialogs; input-bleed guard

## Milestone 2 - real vertical slice

- [ ] Identify and convert one unit's sprite and animation data to a citro2d atlas
- [x] Map the encounter chain columns in `schema.json` (stat order still inferred)
- [x] Resolve a real mission into waves and spawn them into `Battle` (host only; `f3d_inspect`)
- [ ] Verify ATK/DEF/MAG/SPR column order against an observed fight
- [ ] Multi-part monsters: only the body part is spawned today
- [ ] Map `F_MONSTER_SKILL_SET_MST` / `F_MONSTER_SKILL_MST` so enemies use their real actions
- [ ] Load the pack from SD on console and fight a real wave there
- [ ] One exploration area with Circle Pad movement and collision
- [ ] Rewards per upstream `ffbe_battle.py` rules; versioned save with atomic write; reload

## Core work that needs no hardware

- [ ] Mission clear conditions (upstream `_cond_*` list) as tested core rules
- [ ] Drop rolling with seeded statistical tests
- [ ] Save schema draft (`docs/SAVE_FORMAT.md`) after reading upstream `player_state.py`
- [ ] Summon rules: rates, guarantees, step-ups, with deterministic tests
- [ ] Split packs per table (17.9 MB single blob is too big to load whole on console)
- [x] Content validator for the encounter chain (`f3d_inspect validate`)
- [ ] Classify the 863 unresolved missions (inherited upstream gaps vs our bugs)

## Docs still to write

`SETUP.md`, `SAVE_FORMAT.md`, `PERFORMANCE.md` - deliberately not created until there is real content for them.
