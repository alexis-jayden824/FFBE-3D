# Test report

## 2026-11-11 - initial commit

| Level | Result |
| --- | --- |
| Host core tests | **Passed.** `cmake -S . -B build && cmake --build build && ctest --test-dir build` on Linux x86-64, g++, `-Wall -Wextra -Werror`. One test binary, seven test functions: damage formula, chain vs no chain, spark and cap, element and guard, legality/MP/buffs, determinism and outcome, pack read incl. corrupt input. |
| Converter on real data | **Ran.** `tools/assets/mstpack.py exvius-offline-v0_1_30.apk` -> 17 tables, 17.9 MB, 2.5 s. Output not inspected beyond row/column counts; not committed. |
| 3DS compile | **Not run.** No devkitARM in the authoring environment. First verification will be the `build-3dsx` CI job. |
| Emulator | **Not run.** |
| Hardware (New 3DS XL) | **Not run.** Awaiting hardware validation. |

Toolchain pin: not yet established (CI uses `devkitpro/devkitarm:latest`; pin the digest after the first green build).
