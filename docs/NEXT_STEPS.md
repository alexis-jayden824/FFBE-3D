# Next steps

State as of 2026-11-11. Milestone 0 audit is written; Milestone 1 is started.

## Blocked on the owner

1. **Run the first CI build** and report whether `build-3dsx` goes green. The console code has never been compiled.
2. **Run `ffbe3d.3dsx` on the New 3DS XL** (or name an emulator to pin) and report what both screens show.
3. **Decide how decoded unit/ability tables are obtained.** They are in encrypted `.dat` files. Options: (a) run upstream's own `source/exvius/decode_client_data.py` locally and point our converter at its output; (b) defer units and continue with the monster/encounter/equipment tables already readable.
4. **Provide one or two real data packs** (one unit sprite pack, one map pack) locally so asset formats can be identified. Do not commit them.

## Milestone 1 - platform foundation (in progress)

- [x] Dual-screen battle shell with buttons and touch (uncompiled)
- [x] CI: host tests + 3dsx build job with artifact upload
- [ ] First green console build; pin the toolchain image digest
- [ ] SD read of a `.f3dp` pack with a clear error screen when missing or corrupt
- [ ] Audio output (one streamed track, one sound effect) via ndsp
- [ ] Settings file on SD; diagnostics overlay (frame time, heap, linear memory)
- [ ] Sleep/resume and clean exit handling
- [ ] Scene stack + focus navigation in `src/ui/`
- [ ] Separate "edit" and "activate" battle states; confirm dialogs; input-bleed guard

## Milestone 2 - real vertical slice

- [ ] Identify and convert one unit's sprite and animation data to a citro2d atlas
- [ ] Map columns of `F_MONSTER_PARTS_MST` / `F_MONSTER_SKILL_MST` / `F_BATTLE_GROUP_MST` in `schema.json`, verified against data
- [ ] Load one real mission's enemy groups from the pack into `Battle`
- [ ] One exploration area with Circle Pad movement and collision
- [ ] Rewards per upstream `ffbe_battle.py` rules; versioned save with atomic write; reload

## Core work that needs no hardware

- [ ] Mission clear conditions (upstream `_cond_*` list) as tested core rules
- [ ] Drop rolling with seeded statistical tests
- [ ] Save schema draft (`docs/SAVE_FORMAT.md`) after reading upstream `player_state.py`
- [ ] Summon rules: rates, guarantees, step-ups, with deterministic tests
- [ ] Split packs per table; content validator for dangling IDs

## Docs still to write

`SETUP.md`, `SAVE_FORMAT.md`, `PERFORMANCE.md` - deliberately not created until there is real content for them.
