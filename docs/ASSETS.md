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

| F_MONSTER_SKILL_SET_MST | skill_set_id, skill_ids | Upstream `battle_content.py`. Data: every set is 101 comma-separated slots; all 38,053 referenced skill ids exist. |
| F_MONSTER_SKILL_MST | skill_id, name | Upstream; names are readable text. |
| F_MONSTER_SKILL_MST | element_flags | **Verified by correlation with skill names**: eight flags in the order fire, ice, lightning, water, wind, earth, light, dark (e.g. 275 of 324 skills named "Fir..." set only flag 1; "Blizza..." flag 2; "Thund..." flag 3; "Water..." flag 4; "Aero..." flag 5; "Quake" flag 6; "Holy"/"Banish..." flag 7; "Dark..."/"Bio..." flag 8). |
| F_MONSTER_SKILL_MST | attack_type | **Verified by correlation**: 1 = physical (Sweep, Bite, Scratch), 2 = magic (Firaga, Meteor, Flare), 3 = hybrid (blade/fist skills), 4 = none (Store, Focus). |
| F_MONSTER_SKILL_MST | effect_class | **Verified by correlation**: 1 = damage, 2 = protective buffs, 3 = healing, 4 = self/support, 5 = scripted messages. Only class 1 is implemented. |
| F_MONSTER_SKILL_MST | hit_frames | Format `frame:weight:?:?` joined by `-` per hit, `@` between alternatives. Weights sum to 100 in 13,174 of the single-alternative rows. The last two fields are undecoded. Same format as the parts table's damage-frame column. |

| F_UNIT_MST | unit_id, name_jp, rarity, max_level, base_unit_id | One row per rarity form. For one unit's five forms, rarity runs 2-6, max level 30/40/60/80/100, and all forms share one base id. 3,313 rows after merging the later pack's two rows. |
| F_UNIT_MST | hp, mp, atk, def, mag, spr | Each cell is four numbers; the first two rise with rarity and are read as level-1 and max-level values (e.g. HP 378 -> 1144 at rarity 2, 821 -> 2488 at rarity 5). All 3,311 rows parse. Order taken from column order; **consistent with the stats commonly published for that unit, not checked against a running game.** The last two numbers (usually 900,450 for HP) are undecoded. |
| F_UNIT_MST | attack_frames | Same `frame:weight:?:?` format as enemy skills; every row parses. |
| F_TEXT_UNITS_NAME | id, text | Text tables are `{key: "MST_UNIT_NAME_<id>", v: [text, ...]}`; the packer stores id -> first text. 3,092 of 3,311 units get an English name by id or base id. |

421 of 3,243 unit sprites have no row in F_UNIT_MST (mostly the 7-star form of early units); the loader falls back to the nearest lower form and reports it.

Not decoded in the skill table (20 further columns): power, targeting, MP cost, status effects, the effect list, and the AI conditions. Until then loaded skills hit one target at 1.0x power and enemies choose among their damage skills at random.

Things the data showed that the engine must handle: 631 monster parts have HP above 32 bits (maximum 156,784,560,000); groups can hold 25 rows; monsters have up to 4 parts.

Inspect a converted pack on PC:

```
./build/f3d_inspect data/master.f3dp tables
./build/f3d_inspect data/master.f3dp mission <mission id>
./build/f3d_inspect data/master.f3dp validate
```

## Getting the data packs (works today)

The art, maps, scenes and audio are in 19 packs (14.65 GB compressed) listed in the Android project's manifest. Fetch them to a folder on your PC:

```
python tools/assets/fetch_packs.py --list
python tools/assets/fetch_packs.py --dest D:\\FFBE-data
python tools/assets/fetch_packs.py --dest D:\\FFBE-data --only client-assets data-001
python tools/assets/fetch_packs.py --dest D:\\FFBE-data --verify
```

It checks free space, resumes partial downloads, verifies each pack's SHA-256 against the manifest before giving it its final name, and skips packs that are already complete. Keep the folder **outside the repository and outside any cloud-synced folder** (OneDrive, Dropbox): the packs are large, are not yours to redistribute, and do not need unpacking - the tools read the zips in place.

## Archive and art formats (identified from the `client-assets` pack)

Read with `tools/assets/cpk.py` (`list`, `stats`, `extract`):

| What | Format | Evidence |
| --- | --- | --- |
| Container | CRI CPK with `@UTF` tables (plain or XOR-obfuscated), TOC index, optional CRILAYLA compression | `common.cpk` (312 files), `layout.cpk` (1,294 files, obfuscated table, CRILAYLA), `resources_pretutorial.cpk` (27 files) all list and decompress |
| Images | PNG | 193 PNGs in `common.cpk`, 15 in `resources_pretutorial.cpk` |
| Sprite sheets | Cocos `.plist` (XML) next to the PNGs | 5 + 9 files |
| Animations | SpriteStudio `.ssbp` | 1 + 3 files |
| Fonts | bitmap `.fnt` + one OpenType font | 78 `.fnt` in `common.cpk` |
| Screen layouts | `.csv`, one per screen | 1,294 files in `layout.cpk` |
| Audio | CRI `.acb` | 3 files in the pack; codec not inspected |

## What is in the large packs (from the first 7 of 11, file names and sizes only)

| Content | Where | Notes |
| --- | --- | --- |
| Decoded master tables | `data-001`: `static/mst_decoded/*.ndjson` | 219 tables, 172 MB: units, abilities (32 MB), magic, limit bursts, espers, materia, missions, AI (22 MB), towns, map events, quests, story. Readable by `mstpack.py` as-is. **No separate decryption step is needed.** Columns are still opaque keys. |
| Decoded text tables | `data-001`: `static/text_decoded/*.ndjson` | 145 tables, 94 MB. |
| Maps | `native/common_lang/sd/map*/…` | The bulk of the data: tens of MB per chapter folder, CPK archives. Contents not yet opened. |
| Battle backgrounds, effects | `native/common_lang/sd/battlebg`, `battleEffect` | CPK archives; contents not yet opened. |
| Area / banner / summon art | `static/cpk_extract/…` | About 4,700 loose PNG and 260 JPG files. |
| Unit sprites | `native/common_lang/sd/unit/Ver*_unit*_common.cpk` | See below. |
| Limit burst movies | `native/common_lang/sd/unit/Ver*_LBMovie*.cpk`, `.usm` | 102 archives so far, up to 74 MB each. Video; not planned for the first releases. |

### Unit sprite format (one archive opened: 185 units, 3,078 files)

Per unit id:

- `unit_anime_<id>.png` - one sprite sheet. Sizes vary from 512x1024 up to 2000x1984; **106 of 555 PNGs in the archive exceed 1024 on a side**, so sheets must be scaled or re-packed for the console.
- `unit_cgg_<id>.csv` - frame composition: one line per frame, listing the sheet rectangles that make up the frame with offsets and per-part attributes.
- `unit_<name>_cgs_<id>.csv` - one file per animation (`idle`, `standby`, `atk`, `magic_atk`, `magic_standby`, `limit_atk`, `move`, `jump`, `dying`, `dead`, `win_before`, `win`, and a few rarer ones): one line per step with a frame number, an offset and a delay.
- `unit_icon_<id>.png`, `unit_ills_<id>.png` - small icon and portrait.

Field meanings, **verified by converting units and looking at the result** (`unitsprite.py` then `spritepreview.py`, which draws from the converted console file): frames assemble into coherent characters with their effects for every animation checked.

- `cgg` line: anchor, part count, then 11 numbers per part: x, y, flip (bit 0 horizontal, bit 1 vertical), blend (1 = additive), opacity 0-100, rotation in degrees, sheet x, sheet y, width, height, page. Parts are listed top-most first. Across one archive (185 units, 285,154 parts): rotation is always a multiple of 90, page is always 0, 39% of parts are additive, 2,263 frames are empty, 68 parts reference a rectangle outside the sheet (dropped by the converter).
- `cgs` line: frame number, x offset, y offset, delay in 1/60 s.
- Rotation direction is taken as clockwise for positive values. Rotated parts only occur in effect art; a side-by-side render of both directions favoured clockwise but was not conclusive.

### Converted unit sprite (`.f3ds`)

`tools/assets/unitsprite.py` keeps only the sheet rectangles the chosen animations use, packs them into one power-of-two atlas of at most 1024x1024 (shrinking all pieces by one factor if they do not fit), and writes the tables plus the texture in the console's native layout. File layout: `src/content/sprite.h`.

Measured at original size with the default animations (no limit burst): a 640x1280 sheet became a 1024x512 atlas (2.1 MB); 1800-1920 px sheets became 1024x1024 atlases (4.2 MB), two of the five tried having to shrink to 0.72x and 0.85x to fit. With limit burst animations included, one unit dropped to 0.52x.

Not yet inspected: map and background archive contents; the `.ssbp` version; the layout CSV schema; audio.

## Not built yet

- Conversion of PNG / plist / ssbp / layout CSV into console formats (formats identified above; no converter yet).
- Unit sprite and map formats: large packs not yet inspected.
- Audio: ACB container; codec unverified.
- Unit / ability / esper tables: inside encrypted `.dat` files, not in the APK as plain tables.
- Manifests, checksums, split packs, SD layout, free-space checks.

Converted size of the full data set is unmeasured.
