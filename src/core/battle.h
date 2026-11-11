// Portable battle core. No platform headers, no heap after setup, no floats:
// everything is integer / permille so a recorded battle replays identically
// on PC (tests) and on the console.
#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace f3d {

// ---- deterministic RNG (xorshift32) ---------------------------------------
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 0x1234567u) : s(seed ? seed : 1u) {}
    uint32_t next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    // inclusive range
    int range(int lo, int hi) { return lo + int(next() % uint32_t(hi - lo + 1)); }
};

// ---- data ------------------------------------------------------------------
enum Element : uint8_t { Fire, Ice, Lightning, Water, Wind, Earth, Light, Dark, ElementCount };
enum class Side : uint8_t { Party, Enemy };
enum class AbilityKind : uint8_t { Physical, Magic, Hybrid, Heal, Buff, Guard };
enum class Target : uint8_t { OneEnemy, AllEnemies, OneAlly, AllAllies, Self };
enum Stat : uint8_t { ATK, DEF, MAG, SPR, StatCount };

constexpr int kMaxPerSide = 32;   // real enemy groups have up to 25 rows; the party UI caps itself at 6
constexpr int kMaxHits = 32;

struct Ability {
    uint32_t id = 0;
    AbilityKind kind = AbilityKind::Physical;
    Target target = Target::OneEnemy;
    int power_pm = 1000;          // permille of base damage (1000 = 1.0x)
    int mp_cost = 0;
    uint8_t element_mask = 0;     // bit i = Element i
    int ignore_def_pm = 0;        // permille of DEF/SPR ignored
    // Hit timeline: frame offset of each hit from the moment the ability fires,
    // plus the share of total damage that hit carries (permille, sums to 1000).
    uint8_t hit_count = 1;
    std::array<uint16_t, kMaxHits> hit_frame{};
    std::array<uint16_t, kMaxHits> hit_weight_pm{};
    // Buff / debuff payload (kind == Buff). Negative = debuff.
    Stat buff_stat = ATK;
    int buff_pm = 0;
    int buff_turns = 0;
    int lb_fill = 100;            // limit gauge gained per hit landed (centi-crystals)
};

struct Buff { int pm = 0; int turns = 0; };

struct Unit {
    uint32_t id = 0;
    Side side = Side::Party;
    int level = 1;
    int64_t max_hp = 1, hp = 1;                  // real bosses exceed 32 bits
    int max_mp = 0, mp = 0;
    std::array<int, StatCount> stat{};           // ATK DEF MAG SPR
    std::array<int, ElementCount> resist_pm{};   // +1000 = immune, -1000 = double damage
    std::array<Buff, StatCount> buff{};
    bool guarding = false;
    int lb_gauge = 0, lb_max = 1000;
    std::vector<uint32_t> abilities;             // ability ids usable by this unit

    bool alive() const { return hp > 0; }
    int eff(Stat s) const;                       // stat with buffs applied
};

// One queued command. `start_frame` is when the player pressed the button for
// this unit, which is what makes manual chaining possible.
struct Action {
    uint8_t actor = 0;       // index within its side
    uint32_t ability = 0;
    uint8_t target = 0;      // index within the targeted side (ignored for All*/Self)
    uint16_t start_frame = 0;
};

// Chain tunables. Defaults follow the commonly documented behaviour of the
// original; they live here (not in code) so they can be corrected against
// reference footage without touching the resolver.
struct ChainRules {
    int window_frames = 20;      // max gap between hits to keep a chain alive
    int step_pm = 100;           // multiplier gained per chained hit
    int element_step_pm = 200;   // extra per element carried by the hit
    int spark_step_pm = 300;     // extra when two units hit on the same frame
    int cap_pm = 4000;           // 4.0x
    bool same_actor_chains = false;
};

struct HitEvent {
    uint16_t frame;
    Side attacker_side;
    uint8_t attacker, target;
    int64_t damage;      // negative = healing
    int chain_pm;        // multiplier applied to this hit
    int chain_count;     // chain length after this hit
    bool spark;
    bool killed;
};

enum class Outcome : uint8_t { Ongoing, Victory, Defeat };

class Battle {
public:
    explicit Battle(uint32_t seed = 1) : rng(seed) {}

    std::vector<Unit> party, enemies;
    std::vector<Ability> abilities;
    ChainRules chain;
    Rng rng;
    int turn = 1;

    const Ability* find_ability(uint32_t id) const;

    // True if the action is legal right now (actor alive, knows it, has MP, target valid).
    bool can_use(Side side, const Action& a) const;

    // Resolve one side's whole turn. Illegal actions are skipped. Events are
    // appended in frame order so the front end can animate them 1:1.
    void resolve(Side side, const std::vector<Action>& actions, std::vector<HitEvent>& out);

    // Simple built-in enemy turn: every living enemy uses its first ability on
    // a random living party member, staggered so enemies do not chain.
    void enemy_turn(std::vector<HitEvent>& out);

    // Tick buffs, clear guard, advance turn counter. Call after the enemy turn.
    void end_round();

    Outcome outcome() const;

private:
    std::vector<Unit>& units(Side s) { return s == Side::Party ? party : enemies; }
    const std::vector<Unit>& units(Side s) const { return s == Side::Party ? party : enemies; }
    int64_t base_damage(const Unit& atk, const Unit& def, const Ability& ab) const;
};

} // namespace f3d
