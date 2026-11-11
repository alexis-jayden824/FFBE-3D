# Setup

## 1. Get the program

Download the `ffbe3d-3dsx` artifact from the latest green run on the repository's Actions tab, or build it yourself with `make` (devkitPro `3ds-dev` packages). Copy `ffbe3d.3dsx` to `/3ds/` on the SD card and start it from the Homebrew Launcher.

Without any data on the SD card the program still runs and offers a built-in placeholder fight.

## 2. Add real encounters (optional, from data you own)

On a PC with Python 3, in a checkout of this repository:

```
python3 tools/assets/mstpack.py path/to/exvius-offline.apk --preset encounter -o encounter.f3dp
```

This writes a pack of about 2.6 MB (measured on `exvius-offline-v0_1_30.apk`). Copy it to the SD card as:

```
/3ds/ffbe3d/encounter.f3dp
```

The mission select screen then lists every mission that has battles. Do not commit or share the pack; it is derived from game data.

## What the program tells you

| Message on the top screen | Meaning |
| --- | --- |
| Encounter pack loaded | Pack was read and validated. |
| No encounter pack on SD | File not found at the path above. |
| Encounter pack is corrupt or incomplete | The file failed validation; rebuild and recopy it. |
| Encounter pack has an unexpected size / Not enough memory | The file is not a console pack (use `--preset encounter`). |

On the mission screen, "Cannot load" with an id means that mission's data points at something missing from the tables (see `docs/TEST_REPORT.md` for how many).

SD layout so far: only `/3ds/ffbe3d/encounter.f3dp`. Saves, settings and art packs are not implemented.
