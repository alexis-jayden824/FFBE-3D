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

## 3. Build a real party (optional, from data you own)

Needs the data packs (`tools/assets/fetch_packs.py`, see `docs/ASSETS.md`) and Pillow (`python -m pip install pillow`).

Look up unit ids by name, then build the party with one command:

```
python tools/assets/unitsprite.py --packs C:\FFBE-data --find lasswell
python tools/assets/party.py --packs C:\FFBE-data --units 100000107 100000207 100000305
```

`--find` prints the ids that have a sprite; the last two digits are the unit's rarity form (higher is the later art). `party.py` takes one to six ids in party order and writes a folder laid out like the SD card (default `sd_out`):

```
sd_out/3ds/ffbe3d/party.txt          unit ids, one per line
sd_out/3ds/ffbe3d/units.f3dp         stats and names for every unit
sd_out/3ds/ffbe3d/units/<id>.f3ds    one sprite per unit
```

Copy the contents of `sd_out` onto the SD card, merging with the existing `/3ds` folder. To change the party later, run `party.py` again with other ids and recopy; sprites already converted are reused.

What is real for a party unit: its sprite and animations, its English name, its max-level HP / MP / ATK / DEF / MAG / SPR without equipment, and the hit timing of its normal attack. Not yet: abilities, equipment, passives, levels below max.

Some sprite ids have no stats of their own in the data (mostly the 7-star form of early units, such as Rain `100000107`). Those use the stats of the nearest lower form, and the mission screen says how many units are affected.

Each sprite is one texture of up to 1024x1024 (about 4 MB of video memory, and a 4 MB file). If the console runs short, add `--max-atlas 512` (about 1 MB each, less detail).

To check a converted sprite on your PC: `python tools/assets/spritepreview.py sd_out\3ds\ffbe3d\units\<id>.f3ds -o preview.png`.

The older layout (`/3ds/ffbe3d/party/1.f3ds`-`6.f3ds`, sprite only) still works when there is no `party.txt`.

## What the program tells you

| Message on the top screen | Meaning |
| --- | --- |
| Encounter pack loaded | Pack was read and validated. |
| No encounter pack on SD | File not found at the path above. |
| Encounter pack is corrupt or incomplete | The file failed validation; rebuild and recopy it. |
| Encounter pack has an unexpected size / Not enough memory | The file is not a console pack (use `--preset encounter`). |

On the mission screen, "Cannot load" with an id means that mission's data points at something missing from the tables (see `docs/TEST_REPORT.md` for how many).

SD layout so far: `/3ds/ffbe3d/encounter.f3dp`, `units.f3dp`, `party.txt`, `units/<id>.f3ds`. Saves and settings are not implemented.
