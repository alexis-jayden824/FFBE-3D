# Setup

## 1. Get the program

Download the `ffbe3d-3dsx` artifact from the latest green run on the repository's Actions tab, or build it yourself with `make` (devkitPro `3ds-dev` packages). Copy `ffbe3d.3dsx` to `/3ds/` on the SD card and start it from the Homebrew Launcher.

Without any data on the SD card the program still runs and offers a built-in placeholder fight.

## 2. Add real encounters (optional, from data you own)

On a PC with Python 3, in a checkout of this repository:

```
python3 tools/assets/mstpack.py path/to/exvius-offline.apk --preset encounter -o encounter.f3dp
```

This writes a pack of about 4.0 MB (measured on `exvius-offline-v0_1_30.apk`; six tables). Packs built before enemy skills were added (2.6 MB, four tables) still load, but enemies then only use a generic attack - rebuild to get skills. Copy it to the SD card as:

```
/3ds/ffbe3d/encounter.f3dp
```

The mission select screen then lists every mission that has battles. Do not commit or share the pack; it is derived from game data.

## 3. Add real party sprites (optional, from data you own)

Needs the data packs (`tools/assets/fetch_packs.py`, see `docs/ASSETS.md`) and Pillow:

```
python -m pip install pillow
python tools/assets/unitsprite.py --packs C:\FFBE-data --find rain
python tools/assets/unitsprite.py --packs C:\FFBE-data --unit 100000107 -o party\1.f3ds
python tools/assets/spritepreview.py party\1.f3ds -o preview.png
```

`--find` prints the unit ids that have a sprite (the last two digits are the unit's rarity form; higher is the later art). The first run indexes the unit archives and takes a minute or two. `spritepreview.py` draws the converted file on your PC so you can check it before copying.

Convert up to six units as `1.f3ds` to `6.f3ds` (one per party slot) and copy the folder to the SD card:

```
/3ds/ffbe3d/party/1.f3ds ... 6.f3ds
```

Slots without a file keep the placeholder rectangle. Only the sprite is real; the unit's stats are still placeholders.

Each converted unit is one texture of up to 1024x1024 (about 4 MB of video memory, and a 4 MB file). If the console runs short, convert with `--max-atlas 512` (about 1 MB each, less detail). Limit burst animations are left out by default because their effect art is large; `--anims all` includes them.

## What the program tells you

| Message on the top screen | Meaning |
| --- | --- |
| Encounter pack loaded | Pack was read and validated. |
| No encounter pack on SD | File not found at the path above. |
| Encounter pack is corrupt or incomplete | The file failed validation; rebuild and recopy it. |
| Encounter pack has an unexpected size / Not enough memory | The file is not a console pack (use `--preset encounter`). |

On the mission screen, "Cannot load" with an id means that mission's data points at something missing from the tables (see `docs/TEST_REPORT.md` for how many).

SD layout so far: `/3ds/ffbe3d/encounter.f3dp` and `/3ds/ffbe3d/party/1.f3ds`-`6.f3ds`. Saves and settings are not implemented.
