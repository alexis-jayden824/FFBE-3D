# Assets and data

No game content is stored in this repository. You supply it; tools convert it locally into `data/`, which is git-ignored.

## Tables (works today)

```
python3 tools/assets/mstpack.py path/to/exvius-offline.apk -o data/master.f3dp
```

Accepts `.tsv` (with the `# {json}` header line), `.ndjson`, folders, and `.apk`/`.zip` archives containing them. Prints each table's row and column count. Measured on `exvius-offline-v0_1_30.apk`: 17 tables, 17.9 MB, 2.5 s.

Column names are opaque keys in the source. `tools/assets/schema.json` maps them to readable names per table; add a mapping only after verifying it against the data.

## Not built yet

- Sprites, animations, maps, UI art: formats unverified (no CPK pack opened yet).
- Audio: ACB container; codec unverified.
- Unit / ability / esper tables: inside encrypted `.dat` files, not in the APK as plain tables.
- Manifests, checksums, split packs, SD layout, free-space checks.

Converted size of the full data set is unmeasured.
