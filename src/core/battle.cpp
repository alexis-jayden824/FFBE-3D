#include "core/battle.h"
#include <algorithm>

namespace f3d {

int Unit::eff(Stat s) const {
    int64_t v = int64_t(stat[s]) * (1000 + buff[s].pm) / 1000;
    return v < 1 ? 1 : int(v);
}

const Ability* Battle::find_ability(uint32_t id) const {
    for (const auto& a : abilities) if (a.id == id) return &a;
    return nullptr;
}

static bool targets_enemy(Target t) { return t == Target::OneEnemy || t == Target::AllEnemies; }
static Side other(Side s) { return s == Side::Party ? Side::Enemy : Side::Party; }

bool Battle::can_use(Side side, const Action& a) const {
    const auto& us = units(side);
    if (a.actor >= us.size() || !us[a.actor].alive()) return false;
    const Ability* ab = find_ability(a.ability);
    if (!ab) return false;
    const Unit& u = us[a.actor];
    if (std::find(u.abilities.begin(), u.abilities.end(), a.ability) == u.abilities.end()) return false;
    if (u.mp < ab->mp_cost) return false;
    if (ab->target == Target::OneEnemy) {
        const auto& ts = units(other(side));
        return a.target < ts.size() && ts[a.target].alive();
    }
    if (ab->target == Target::OneAlly) return a.target < us.size() && us[a.target].alive();
    return true;
}

// Damage before chain, variance and hit split.
//   physical: ATK^2 / DEF      magic: MAG^2 / SPR      hybrid: average of both
// then * power * (1 + level/100) * element modifier, halved if the target guards.
int Battle::base_damage(const Unit& atk, const Unit& def, const Ability& ab) const {
    auto one = [&](Stat off, Stat dfn) -> int64_t {
        int64_t o = atk.eff(off);
        int64_t d = int64_t(def.eff(dfn)) * (1000 - ab.ignore_def_pm) / 1000;
        if (d < 1) d = 1;
        return o * o / d;
    };
    int64_t dmg = 0;
    switch (ab.kind) {
        case AbilityKind::Physical: dmg = one(ATK, DEF); break;
        case AbilityKind::Magic:    dmg = one(MAG, SPR); break;
        case AbilityKind::Hybrid:   dmg = (one(ATK, DEF) + one(MAG, SPR)) / 2; break;
        default: return 0;
    }
    dmg = dmg * ab.power_pm / 1000;
    dmg = dmg * (100 + atk.level) / 100;

    // Element modifier: average resistance across the elements the hit carries.
    int n = 0, sum = 0;
    for (int e = 0; e < ElementCount; ++e)
        if (ab.element_mask & (1u << e)) { sum += def.resist_pm[e]; ++n; }
    if (n) {
        int mod = 1000 - sum / n;
        if (mod < 0) mod = 0;
        dmg = dmg * mod / 1000;
    }
    if (def.guarding) dmg /= 2;
    return dmg > 0x3fffffff ? 0x3fffffff : int(dmg);
}

namespace {
struct Pending {
    uint16_t frame;
    uint8_t actor, target, hit_index;
    uint8_t order;           // queue order, for stable tie-breaking
    const Ability* ab;
    int total;               // full-ability damage against this target
};
struct ChainState { int count = 0; int last_frame = -1000; int last_actor = -1; };
}

void Battle::resolve(Side side, const std::vector<Action>& actions, std::vector<HitEvent>& out) {
    auto& us = units(side);
    auto& them = units(other(side));
    std::vector<Pending> hits;
    uint8_t order = 0;

    for (const Action& a : actions) {
        if (!can_use(side, a)) continue;
        const Ability& ab = *find_ability(a.ability);
        Unit& u = us[a.actor];
        u.mp -= ab.mp_cost;

        if (ab.kind == AbilityKind::Guard) { u.guarding = true; continue; }

        // Collect target indices.
        uint8_t idx[kMaxPerSide]; int n = 0;
        auto& pool = targets_enemy(ab.target) ? them : us;
        switch (ab.target) {
            case Target::OneEnemy: case Target::OneAlly: idx[n++] = a.target; break;
            case Target::Self: idx[n++] = a.actor; break;
            default:
                for (size_t i = 0; i < pool.size() && n < kMaxPerSide; ++i)
                    if (pool[i].alive()) idx[n++] = uint8_t(i);
        }

        if (ab.kind == AbilityKind::Buff) {
            for (int i = 0; i < n; ++i) pool[idx[i]].buff[ab.buff_stat] = { ab.buff_pm, ab.buff_turns };
            continue;
        }
        if (ab.kind == AbilityKind::Heal) {
            for (int i = 0; i < n; ++i) {
                Unit& t = pool[idx[i]];
                int heal = int(int64_t(u.eff(SPR)) * ab.power_pm / 1000 * (100 + u.level) / 100);
                heal = std::min(heal, t.max_hp - t.hp);
                t.hp += heal;
                out.push_back({ a.start_frame, side, a.actor, idx[i], -heal, 1000, 0, false, false });
            }
            continue;
        }

        // Damage: one roll of variance per ability use per target (85-100%).
        for (int i = 0; i < n; ++i) {
            int total = int(int64_t(base_damage(u, them[idx[i]], ab)) * rng.range(850, 1000) / 1000);
            for (uint8_t h = 0; h < ab.hit_count; ++h)
                hits.push_back({ uint16_t(a.start_frame + ab.hit_frame[h]), a.actor, idx[i], h, order, &ab, total });
        }
        ++order;
    }

    std::stable_sort(hits.begin(), hits.end(), [](const Pending& x, const Pending& y) {
        return x.frame != y.frame ? x.frame < y.frame : x.order < y.order;
    });

    // Chain state is tracked per target, as in the original.
    std::array<ChainState, kMaxPerSide> cs{};
    for (const Pending& p : hits) {
        Unit& t = them[p.target];
        if (!t.alive()) continue;   // overkill hits are dropped
        ChainState& c = cs[p.target];

        int gap = int(p.frame) - c.last_frame;
        bool different = c.last_actor != int(p.actor);
        bool linked = c.count > 0 && gap <= chain.window_frames && (different || chain.same_actor_chains);
        bool spark = linked && gap == 0 && different;

        int mult = 1000;
        if (linked) {
            // Multiplier is a function of chain length so far, not accumulated
            // drift: 1.0 + count*step (+ element / spark bonuses on this hit).
            int elems = 0;
            for (int e = 0; e < ElementCount; ++e) if (p.ab->element_mask & (1u << e)) ++elems;
            mult += c.count * (chain.step_pm + elems * chain.element_step_pm);
            if (spark) mult += chain.spark_step_pm;
            if (mult > chain.cap_pm) mult = chain.cap_pm;
            ++c.count;
        } else {
            c.count = 1;
        }
        c.last_frame = p.frame;
        c.last_actor = p.actor;

        int dmg = int(int64_t(p.total) * p.ab->hit_weight_pm[p.hit_index] / 1000 * mult / 1000);
        if (dmg < 1) dmg = 1;
        t.hp = std::max(0, t.hp - dmg);

        Unit& u = us[p.actor];
        u.lb_gauge = std::min(u.lb_max, u.lb_gauge + p.ab->lb_fill);

        out.push_back({ p.frame, side, p.actor, p.target, dmg, mult, c.count, spark, !t.alive() });
    }
}

void Battle::enemy_turn(std::vector<HitEvent>& out) {
    std::vector<Action> acts;
    uint16_t frame = 0;
    for (size_t i = 0; i < enemies.size(); ++i) {
        const Unit& e = enemies[i];
        if (!e.alive() || e.abilities.empty()) continue;
        uint8_t alive[kMaxPerSide]; int n = 0;
        for (size_t j = 0; j < party.size() && n < kMaxPerSide; ++j)
            if (party[j].alive()) alive[n++] = uint8_t(j);
        if (!n) break;
        acts.push_back({ uint8_t(i), e.abilities[0], alive[rng.range(0, n - 1)], frame });
        frame = uint16_t(frame + 60);   // one second apart: no enemy chains
    }
    resolve(Side::Enemy, acts, out);
}

void Battle::end_round() {
    for (auto* side : { &party, &enemies })
        for (Unit& u : *side) {
            u.guarding = false;
            for (Buff& b : u.buff)
                if (b.turns > 0 && --b.turns == 0) b.pm = 0;
        }
    ++turn;
}

Outcome Battle::outcome() const {
    auto any_alive = [](const std::vector<Unit>& v) {
        return std::any_of(v.begin(), v.end(), [](const Unit& u) { return u.alive(); });
    };
    if (!any_alive(enemies)) return Outcome::Victory;
    if (!any_alive(party)) return Outcome::Defeat;
    return Outcome::Ongoing;
}

} // namespace f3d
