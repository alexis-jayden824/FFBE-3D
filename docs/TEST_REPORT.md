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
