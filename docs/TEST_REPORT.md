# Test report

## 2026-11-11 - initial commit

| Level | Result |
| --- | --- |
| Host core tests | **Passed** locally and in CI (same run). `cmake -S . -B build && cmake --build build && ctest --test-dir build` on Linux x86-64, g++, `-Wall -Wextra -Werror`. One test binary, seven test functions: damage formula, chain vs no chain, spark and cap, element and guard, legality/MP/buffs, determinism and outcome, pack read incl. corrupt input. |
| Converter on real data | **Ran.** `tools/assets/mstpack.py exvius-offline-v0_1_30.apk` -> 17 tables, 17.9 MB, 2.5 s. Output not inspected beyond row/column counts; not committed. |
| 3DS compile | **Passed in CI.** GitHub Actions run 43006702872 on commit `eef80ab`: `make` in `devkitpro/devkitarm:latest` produced `ffbe3d.3dsx` (uploaded as artifact `ffbe3d-3dsx`). Compiling is not evidence that it runs. |
| Emulator | **Not run.** |
| Hardware (New 3DS XL) | **Not run.** Awaiting hardware validation. |

Toolchain pin: not yet established (CI uses `devkitpro/devkitarm:latest`; pin the digest after the first green build).

## 2026-11-11 - encounter loading; first hardware boot

| Level | Result |
| --- | --- |
| Hardware (New 3DS XL) | **Boots.** Owner ran `ffbe3d.3dsx` from the first CI build (commit `eef80ab`) and reported that it starts and shows the placeholder graphics. Not yet reported: whether buttons, touch, chaining and exit behave correctly; frame rate; sleep/resume. Reported by the owner, not captured. |
| Host tests | **Passed** (2 test binaries: `core`, `encounter`). Encounter tests use invented fixture tables with the real table names and column keys. |
| Encounter chain on real data | **Ran** `f3d_inspect master.f3dp validate` on the pack built from `exvius-offline-v0_1_30.apk`: 21,427 missions in the phase table; 14,444 resolve end to end (32,087 waves, largest 25 enemies); 6,120 have no battle phases; 66 point at an empty lottery id; 797 reference a monster with no stats row. An independent Python pass over the same tables agrees (863 unresolved of 15,307 missions with battle phases). |
| 3DS compile of this commit | See the CI run for this commit. |
| Emulator | **Not run.** |

## 2026-11-11 - mission select and real waves on console

| Level | Result |
| --- | --- |
| Hardware (New 3DS XL), previous build | Owner reported the placeholder battle shell "runs good" (commit `eef80ab` build). No itemised input results were given; controls from that build are marked (hw) in CONTROLS.md on that basis. |
| Host tests | **Passed** (`core`, `encounter`), including mission listing and enemy names. |
| Console pack on real data | **Ran** `mstpack.py --preset encounter` on the owner's APK: 4 tables, 2.6 MB. `f3d_inspect validate` on that pack gives the same numbers as the full pack (14,444 missions resolve). |
| 3DS compile of this commit | See the CI run for this commit. |
| Hardware, this build | **Not run.** New and untested on console: SD pack loading and its error messages, mission select, multi-wave battles with up to 25 enemies, retreat dialog, diagnostics overlay, Japanese monster names in the system font. |

## 2026-11-11 - enemy skills

| Level | Result |
| --- | --- |
| Hardware (New 3DS XL), previous build (`ee4ddc0`) | Owner built the pack on Windows (4 tables, 2.6 MB, matching our numbers), copied it and the build to SD, and reported "It worked". No itemised answers were given for pack message, name rendering, wave progression or frame time, so those remain unconfirmed individually. |
| Host tests | **Passed** (`core`, `encounter` incl. skill parsing, skill loading, random skill choice, event ability ids). |
| Skills on real data | **Ran** `f3d_inspect validate` on the 6-table console pack (4.0 MB): 14,732 monster parts have a skill set and 2,415 do not; 76,891 skill slots listed, 49,762 loaded as damage skills, 27,129 are non-damage (not implemented), 0 missing; 1,043 parts with a set have no damage skill and fall back to a generic attack. |
| 3DS compile of this commit | See the CI run for this commit. |
| Hardware, this build | **Not run.** New on console: enemy skills loaded per wave, skill names shown after the enemy turn. |

## 2026-11-11 - unit sprites

| Level | Result |
| --- | --- |
| Hardware (New 3DS XL), previous builds | Owner reported the mission-select build "worked" (`ee4ddc0`). The enemy-skill build (`263c157`) has not been reported on. Itemised checks (Japanese names, frame time, wave progression) remain unanswered. |
| Host tests | **Passed** (`core`, `encounter`, `sprite`). The sprite fixture is a synthetic unit in the game's source format; the fixture script converts it and checks the output texture pixel-for-pixel, and the C++ test checks the reader, the animator and rejection of malformed files. |
| Converter on real data | **Ran** on five real units from the packs; previews drawn from the converted files were inspected by eye and show correctly assembled characters and effects (not committed). |
| 3DS compile of this commit | See the CI run for this commit. |
| Hardware, this build | **Not run.** New and untested on console: loading `.f3ds` files, texture upload and orientation, flipped / rotated / additive / translucent parts, animation playback, memory use with six 4 MB textures. Texture orientation and rotation direction are the likeliest things to be wrong on first run. |

## 2026-11-11 - first hardware run of unit sprites: garbled, fixed

| Level | Result |
| --- | --- |
| Hardware (New 3DS XL), build `5350beb` | **Ran; sprites garbled.** Owner converted Rain (100000107) on Windows (numbers matched ours), and sent two photos and a video. Observed: six sprites load and animate at the party positions, but each frame is assembled from the wrong, upside-down pieces of the atlas. Also confirmed from the photos: Japanese monster names render in the system font; wave counter, chain counter and enemy HP display work; six 4 MB textures load. |
| Cause | The converter wrote the texture bottom row first. The GPU reads memory row 0 as the top (v = 1), so every piece was sampled from the vertically mirrored place in the atlas. The PC preview could not catch this because it un-flipped with the same assumption. |
| Fix | Texture is now written top row first; file version bumped to 2 so version 1 files are rejected rather than drawn wrong. Host tests pass. **The fix has not yet been run on hardware.** |
| Still unverified on hardware | Rotation direction, additive blending and mirrored parts cannot be judged from the garbled run. Frame time and `linear free` not yet reported. |
