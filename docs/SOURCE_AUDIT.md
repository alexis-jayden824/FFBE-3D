# Source audit (Milestone 0)

Audited 2026-11-11.

- Upstream: https://github.com/Elvlin/exvius-offline
- Pinned revision: `b0b73648ceb1a3783105ed2104600be651c9c515` (2026-11-11 08:11 +0700, "Update README.md"), shallow clone
- Also inspected: `exvius-offline-v0_1_30.apk` supplied by the owner (106,360,530 bytes; newer than the newest APK in the repo's `release/`, which is v0.1.25)

Paths below are relative to the upstream repo unless prefixed `apk:`.

## 1. What upstream is

**It is the original Android client binary, patched, talking to a local re-implementation of the game server. It is not a reconstructed client.**

| Finding | Evidence |
| --- | --- |
| Build starts from the original Global v9.0.0 APK (SHA-256 `955deacf…ad246`), which is not in the repo | `source/README.md` "Building" |
| The original native client ships inside the release APK | `apk:lib/arm64-v8a/libgame.so` (64,454,732 bytes, arm64 only) |
| The client is hooked at runtime with Frida Gadget and JavaScript | `source/README.md` (Frida Gadget 17.24.3); `source/memorial/frida_script.js` (208 KB); `source/exvius/*_menu.js`, `android_display.js`, `native_bridge.js` |
| The client's network requests are answered on-device by Python run through Chaquopy | `source/README.md` (Chaquopy 16.1.0); `source/memorial/android/src/main/python/android_game.py`; `…/LocalGameConnection.java`, `LocalGameBridge.java` |
| The server side is re-implemented in Python | `source/server/ffbe_player.py` (251 KB), `player_state.py` (318 KB), `ffbe_battle.py`, `ffbe_gacha.py`, `exvius_actions_*.py`, `endpoints.json`, `responses.json` |
| A second server implementation exists in Rust | `source/server_app/src/main.rs`, `Cargo.toml` |
| Wire protocol is documented | `source/docs/analysis/FFBE_SERVER_CONTRACT.md`, `source/docs/reference/tbmonkey_api_calls/*.md` |
| Engine is Cocos | `source/README.md` ("original Cocos asset-path getter"); `…/LocalCocosTasks.java` |

### Consequence for this project

Everything the player sees and plays - rendering, UI, exploration maps, the battle simulation itself, animation playback, audio - lives **only in `libgame.so`**, a proprietary 64-bit ARM binary with no source. None of it can be compiled for the 3DS (32-bit ARM11).

What upstream's source does contain is the **server half**: player state, rewards, drops, mission clear conditions, summons, shops, raids, expeditions, login calendar. `source/server/ffbe_battle.py` describes itself as "everything that happens *because a battle was fought*" (drops, mission-end reply, challenge payment) - it settles results; it does not simulate combat.

So the 3DS project is:

- **Client: full re-implementation** (no reusable code exists).
- **Server-side rules: behavioural reference** from upstream Python, re-expressed in our portable core.

This is the audit outcome the brief asked for in section 2: *primarily a reimplementation, not a source port.*

## 2. Licensing

- **No license file exists in the upstream repo** (searched for `LICENSE*`, `COPYING*`, `*licen*`). Public visibility is not a reuse license, so upstream code is **read for behaviour only and not copied**.
- Game content (binary, art, audio, text, master data) belongs to Square Enix / gumi, as upstream's README states.
- Decision: this repository contains only original engine code, tools and docs. Converter inputs and outputs are git-ignored (`.gitignore`). Tests use original placeholder fixtures.

## 3. Game data

| Finding | Evidence |
| --- | --- |
| 19 downloadable packs, 14.65 GB compressed, 14.95 GB unpacked | `data/latest.json` (schema 1, revision 9, version `2026.11.04.9`) |
| Packs are hosted as GitHub release assets | `data/latest.json` URLs; `data/README.md` |
| Map / town / event content is CRI **CPK** packs; music is **ACB** | `source/docs/reference/tbmonkey_api_calls/missing_cdn_packs.md`; `apk:assets/rules/native_content.zip` contains `.cpk` |
| 4,781 packs referenced, 57 had no known source as of 2026-10-11 (event layer only; main story complete); partly recovered since | same file |
| Master tables ship as versioned encrypted `.dat`; keys are read out of the original binary | `source/exvius/decode_client_data.py` (AES CBC/ECB; key triplets located in `libgame.so`) |
| Column names in master tables are opaque 8-character keys | `apk:assets/rules/content/battle/*.tsv` header lines |
| The APK itself contains **no sprites, maps or audio** - only tables, text, and 73 banner PNGs | `apk:` listing: 54 json, 8 tsv, 73 png, 1 zip under `assets/` |

### Tables readable from the APK today

Packed with `tools/assets/mstpack.py` (17 tables, 17.9 MB output, 2.5 s):

| Table | Rows | Cols |
| --- | ---: | ---: |
| F_BATTLE_GROUP_MST | 59,441 | 7 |
| F_BATTLE_LOTTERY_MST | 30,076 | 3 |
| F_MISSION_MST | 22,603 | 2 |
| F_MISSION_PHASE_MST | 39,993 | 16 |
| F_MONSTER_MST | 16,937 | 3 |
| F_MONSTER_PARTS_MST | 17,147 | 51 |
| F_MONSTER_SKILL_MST | 17,926 | 26 |
| F_MONSTER_SKILL_SET_MST | 5,305 | 2 |
| F_EQUIP_ITEM_MST | 4,277 | 40 |
| F_EQUIP_ITEM_EXPLAIN_MST | 4,270 | 3 |
| F_ICON_MST | 10,800 | 3 |
| F_RECIPE_MST | 2,053 | 14 |
| F_RECIPE_BOOK_MST | 2,050 | 8 |
| F_TEXT_ITEM_EQUIP_NAME / SHORT / LONG | ~3,910 each | 2 |
| F_TEXT_UNITS_NAME | 3,319 | 2 |

Monster and encounter tables are from a JP source (`"oracle": "jp_battle_oracle/decoded"` in the headers; monster names are Japanese).

**Not in the APK:** unit stats, player abilities/magic, limit bursts, espers, materia, story/map scripts. Those are inside the encrypted `.dat` files and the 14.65 GB download.

## 4. Unverified (not yet inspected)

- Unit sprite and map formats inside the large `data-0NN` packs. (The `client-assets` pack has since been opened: CPK container, PNG, Cocos plist, SpriteStudio ssbp, bitmap fonts, CSV layouts - see ASSETS.md.)
- Audio codec inside ACB (HCA is typical for CRI but not confirmed here).
- Meaning of almost every table column (`tools/assets/schema.json` is empty until columns are verified).
- Upstream save schema (`source/server/player_state.py`, `ffbe_state.py` not yet read in detail).
- Battle formulas: the core's current formulas come from general community knowledge, **not** from upstream evidence, because upstream has no battle simulation source.
- Whether upstream builds from a clean checkout (needs Windows, the original APK and vendor tools that are not in the repo; not attempted).

## 5. Feasibility verdict

Feasible as a long reimplementation, with three hard dependencies:

1. **Asset formats** must be decodable on a PC (CPK container, sprite/animation format, audio). Until one real pack is converted and drawn on a 3DS, visual parity is unproven.
2. **Battle rules** have no source of truth in upstream. Parity has to be established from the master data plus observation of the Android build.
3. **Volume**: 59k battle groups, 17k monsters, 18k monster skills. The engine must be data-driven; hand-porting content is not an option.
