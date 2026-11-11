# Feature parity matrix

Statuses: **verified upstream** (seen in upstream source/data), **unverified**, **implemented**, **host-tested**, **emulator-tested**, **hardware-tested**, **partial**, **blocked**.

"Source" paths are in the pinned upstream revision (see SOURCE_AUDIT.md). `libgame.so` means the behaviour exists only in the proprietary client binary, so there is no readable source of truth.

Last updated 2026-11-11. Nothing has run on an emulator or a console yet.

## Battle

| Feature | Source | Upstream status | Our status | Test plan |
| --- | --- | --- | --- | --- |
| Turn resolution, hit timeline | `libgame.so` | unverified | partial, host-tested (`tests/core`) | Replay known fights from the Android build |
| Physical / magic / hybrid damage | `libgame.so` | unverified | partial, host-tested; formula from community knowledge | Golden values captured from Android build |
| Chaining, spark, cap | `libgame.so` | unverified | partial, host-tested; tunables in `ChainRules` | Frame-timed captures |
| Elements, resistances | `libgame.so`; `F_MONSTER_PARTS_MST` | unverified | partial, host-tested | Column mapping + golden values |
| Buffs, debuffs, guard | `libgame.so` | unverified | partial, host-tested | per-effect golden tests |
| Ailments, breaks, dispel, killers, cover, counters, mitigation | `libgame.so` | unverified | not started | per-effect golden tests |
| Limit bursts, espers, dual wield, multi-cast, Brave Shift | `libgame.so` | unverified | not started (gauge fill only) | audit each individually |
| Enemy AI scripts, thresholds, phases | `source/server/content/battle/F_MONSTER_SKILL_SET_MST.tsv`, `F_MONSTER_SKILL_MST.tsv` | verified upstream (data present) | placeholder AI only | decode skill-set format; scripted-boss replays |
| Encounters, waves | `F_BATTLE_GROUP_MST`, `F_MISSION_PHASE_MST`, `F_BATTLE_LOTTERY_MST` | verified upstream (data present) | tables packed, not interpreted | content validator: every mission resolves to groups |
| Drops and result settlement | `source/server/ffbe_battle.py` (`roll_drops`, `apply_result_drops`) | verified upstream | not started | port rules; statistical tests with seeded RNG |
| Mission clear conditions | `source/server/ffbe_battle.py` (`_cond_*`, `CONDITION_CHECKS`) | verified upstream | not started | one test per condition |
| Auto / repeat / speed | `source/exvius/mission_repeat.js`, `source/server/exvius_repeat.py` | verified upstream | not started | - |

## Story and exploration

| Feature | Source | Upstream status | Our status | Test plan |
| --- | --- | --- | --- | --- |
| Maps, collision, NPCs, chests | CPK packs; `source/server/content/field_treasures.json`, `exvius_field_treasures.py` | verified upstream (served); format unverified | blocked on asset format | open one map pack |
| Dialogue text (EN) | `source/server/content/dialogue_en.json` | verified upstream | not started | localization validator |
| Story progression flags ("switches") | `source/server/ffbe_battle.py` (`grant_cleared_mission`, `_switch_container`) | verified upstream | not started | save round-trip tests |
| Cutscenes | CPK packs / `libgame.so` | unverified | blocked on asset format | - |

## Units, economy, events

| Feature | Source | Upstream status | Our status | Test plan |
| --- | --- | --- | --- | --- |
| Player state, units, inventory | `source/server/player_state.py`, `ffbe_player.py`, `ffbe_state.py` | verified upstream | not started | schema doc + migration tests |
| Unit stats / abilities master data | encrypted `.dat` (`source/exvius/decode_client_data.py`) | verified upstream | blocked: not in APK | decode locally, then pack |
| Equipment and recipes | `F_EQUIP_ITEM_MST`, `F_RECIPE_MST` (in APK) | verified upstream (data present) | tables packed, not interpreted | column mapping |
| Awakening, fusion, trust | `ffbe_player.py`; release notes v0.1.30 | verified upstream (with known bug history) | not started | transaction tests |
| Summons: banners, rates, step-ups, guarantees | `source/server/ffbe_gacha.py`, `gacha_rates.py`, `ffbe_banner.py`, `gacha_*.json` | verified upstream | not started | guarantee tests; rate sanity with seeded RNG |
| Replayable Lapis objectives | `source/server/exvius_repeat.py`, `source/exvius/offline.json` | verified upstream | not started | repeat-clear tests |
| Shops, exchange, bundles for Lapis | `exvius_actions_shops.py`, `exvius_shop_catalogue.py`, `global_bundles.json` | verified upstream | not started | currency invariants |
| Events / Vortex filters / seasons | `season.py`, `seasons.json`, `event_archive.py`, `exvius_events.py` | verified upstream; "Event coverage is unverified" per `offline.json` | not started | quest-graph validator |
| Raids (solo-adjusted) | `exvius_raid_progress.py`, `source/exvius/raid/` | verified upstream | not started | - |
| Arena, colosseum | `exvius_arena.py`, `exvius_colosseum.py` | verified upstream | not started | - |
| Expeditions, esper boards, login calendar, daily quests | `exvius_actions_expedition.py`, `login_calendar.json`, `daily_quests.json` | verified upstream | not started | injected-clock tests incl. clock moved backward |
| Friends / companions | `ffbe_friends.py`, `friends.yaml` | verified upstream (offline stand-in) | not started | - |

## Platform

| Feature | Our status | Test plan |
| --- | --- | --- |
| Dual-screen layout, buttons, touch | implemented (`src/platform/3ds/main.cpp`), compiles in CI, **not run** | CI build, then emulator, then hardware |
| Table pack format + reader | implemented, host-tested incl. corrupt input | - |
| Table converter | implemented; run against real APK (17 tables) | fixture test in CI |
| Saves, audio, SD content loading, sleep/resume | not started | per brief section 10 and 12 |
