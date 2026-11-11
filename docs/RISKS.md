# Risk register

| # | Risk | Impact | Likelihood | Mitigation / next evidence |
| --- | --- | --- | --- | --- |
| 1 | **No source of truth for battle rules.** The simulation exists only in the proprietary client binary. | Parity cannot be proven by reading code. | Certain | Build golden tests from the Android build's observable results; treat every formula as unverified until matched. |
| 2 | **Asset formats partly known.** The container (CPK) is readable and UI art is PNG / plist / SpriteStudio / CSV layouts. Unit sprites and maps (large packs) are not yet inspected, and nothing has been converted or drawn on console. | Visual parity unproven. | Medium | Convert one background, one UI sheet, then one unit, and draw them on console. |
| 3 | **Memory.** Original art targets phones; 3DS textures are small and application RAM is limited. Budgets are unmeasured. | Battles with six animated units plus effects may not fit. | High | Measure on hardware early; scale variants and atlas limits in the converter. Unit sheets go up to 2000x1984 against a 1024x1024 console limit, so every large sheet needs scaling or re-packing. |
| 4 | **Data volume.** 14.95 GB unpacked on Android. Converted size is unmeasured. | SD size and conversion time. | Medium | Measure after converting representative packs; split files below FAT32 limits. |
| 5 | **Content rights.** Game data belongs to Square Enix / gumi; upstream has no license. | Takedown of anything that redistributes content or copied code. | Medium | Code-only repo; converter runs on user-supplied data; upstream read for behaviour only. |
| 6 | ~~Encrypted master data.~~ **Resolved 2026-11-11:** the `data-001` pack ships 219 master tables already decoded as ndjson, including units and abilities. Column meanings are still opaque. | - | - | Map columns as they are verified. |
| 7 | **No hardware or emulator in the build environment.** | Console code is uncompiled and unrun. | Certain today | CI build first; owner runs builds on the New 3DS XL and reports. |
| 8 | **Scope.** 59k battle groups, 17k monsters, 18k enemy skills, years of events. | "Complete and bug-free" is not a state that can be asserted. | Certain | Coverage tracked per feature in FEATURE_PARITY.md; content-wide validators rather than manual checks. |
| 9 | **Upstream is itself unfinished** (README: some upgrades and event interactions need fixes; `offline.json`: event coverage unverified; 57 packs had no source). | Inherited gaps. | High | Record inherited defects separately from port regressions. |
| 10 | Opaque column keys. | Slow table interpretation. | High | Map columns only when verified; schema file grows incrementally. |
