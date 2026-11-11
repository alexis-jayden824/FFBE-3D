# Assets and data

No game content is stored in this repository. You supply it; tools convert it locally into `data/`, which is git-ignored.

## Tables (works today)

```
python3 tools/assets/mstpack.py path/to/exvius-offline.apk -o data/master.f3dp
```

Accepts `.tsv` (with the `# {json}` header line), `.ndjson`, folders, and `.apk`/`.zip` archives containing them. Prints each table's row and column count. Measured on `exvius-offline-v0_1_30.apk`: 17 tables, 17.9 MB, 2.5 s.

Column names are opaque keys in the source. `tools/assets/schema.json` maps them to readable names per table; add a mapping only after verifying it against the data.

## Column mappings and their evidence

| Table | Mapped columns | Evidence |
| --- | --- | --- |
| F_MISSION_PHASE_MST | mission_id, phase_order, phase_type, lottery_id | Upstream `source/server/battle_content.py` documents the chain. Data check: every phase whose id resolves in the lottery table has `phase_type` 2 (33,263 of 33,348); none of type 1 do. |
| F_BATTLE_LOTTERY_MST | lottery_id, weight, group_id | Same source; 22,273 lotteries have one member, the rest are weighted sets of up to 21. |
| F_BATTLE_GROUP_MST | group_id, monster_id, slot_order | Same source; up to 25 rows per group. |
| F_MONSTER_PARTS_MST | monster_id, part_num, name, hp, mp, level, skill_set_id, break_id | Same source (part rows share a monster id; part 1 is the body; level and HP are what upstream rescales for event difficulty). |
| F_MONSTER_PARTS_MST | atk, def, mag, spr | **Inferred from column position only.** Upstream rescales these four together but never names them. Treat as unverified. |

Things the data showed that the engine must handle: 631 monster parts have HP above 32 bits (maximum 156,784,560,000); groups can hold 25 rows; monsters have up to 4 parts.

Inspect a converted pack on PC:

```
./build/f3d_inspect data/master.f3dp tables
./build/f3d_inspect data/master.f3dp mission <mission id>
./build/f3d_inspect data/master.f3dp validate
```

## Not built yet

- Sprites, animations, maps, UI art: formats unverified (no CPK pack opened yet).
- Audio: ACB container; codec unverified.
- Unit / ability / esper tables: inside encrypted `.dat` files, not in the APK as plain tables.
- Manifests, checksums, split packs, SD layout, free-space checks.

Converted size of the full data set is unmeasured.
