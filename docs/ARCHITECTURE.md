# Architecture

Decision (2026-11-11): C++17 on devkitARM + libctru + citro2d, with a portable core that also builds and is tested on a PC. Justified by the audit: there is no upstream client source to port, so the client is written natively, and the only way to keep thousands of data-driven mechanics correct is a core that can be tested without a console.

```
src/core/          battle rules, later: economy, quests, saves. No platform headers.
src/content/       read-only access to converted data packs (.f3dp), zero-copy.
src/platform/3ds/  libctru/citro2d: rendering, input, audio, SD, lifecycle.
src/ui/            (planned) scenes and focus navigation, one layout per screen.
tools/assets/      PC-side converters. Read data you supply; write to data/ (git-ignored).
tests/core/        host tests for rules.        tests/content/  fixtures + pack tests.
```

## Rules the code follows

- **Determinism.** The core uses integers and permille only, with an injected seed. The same seed and inputs produce the same battle on PC and console (`test_determinism_and_outcome`).
- **Simulation clock is separate from rendering.** Chain timing is counted in 60 Hz simulation frames (`Action::start_frame`, `Ability::hit_frame`). A slow renderer must not change damage.
- **The core emits events; the platform draws them.** `Battle::resolve` returns `HitEvent`s in frame order.
- **Tunables are data.** Chain window, steps and cap live in `ChainRules` so they can be corrected against evidence without touching the resolver.
- **Source IDs are preserved.** Packs keep original numeric IDs as column 0 and are binary-searched by ID.
- **Data is read in place.** `.f3dp` packs are one blob with validated extents; nothing is parsed into heap structures at boot.
- **No game content in git.**

## Pack format (`.f3dp` v1)

Documented at the top of `src/content/pack.h`. Integer columns are stored as i32; anything else as an offset into a shared string area. Composite cells such as `40:1000370:1` stay as strings until their meaning is verified.

Known limit to fix before real content loads on console: a single pack is loaded whole. The full table set is 17.9 MB today; it will be split per table and memory-mapped or paged as tables grow.

## Screen roles

See brief section 7. Implemented so far: battle only (top: field; bottom: six command cards).
