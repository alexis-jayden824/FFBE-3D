#include "content/encounter.h"
#include <algorithm>

namespace f3d {

const char* to_string(EncounterError e) {
    switch (e) {
        case EncounterError::Ok: return "ok";
        case EncounterError::MissingTable: return "a required table is missing from the pack";
        case EncounterError::MissingColumn: return "a required column is missing (schema out of date?)";
        case EncounterError::NoBattlePhases: return "mission has no battle phases";
        case EncounterError::NoLottery: return "phase points at a lottery id with no members";
        case EncounterError::NoGroup: return "lottery points at a battle group with no slots";
        case EncounterError::NoMonsterStats: return "group slot points at a monster with no stats row";
    }
    return "?";
}

namespace {
constexpr int kBattlePhase = 2;   // phase_type value used by every phase that resolves to a lottery

struct Cols { int v[12]; bool ok; };
Cols cols(const Table& t, std::initializer_list<const char*> names) {
    Cols c{}; c.ok = t.valid(); int n = 0;
    for (const char* name : names) { c.v[n] = t.valid() ? t.column(name) : -1; if (c.v[n] < 0) c.ok = false; ++n; }
    return c;
}
int clamp_int(int64_t v) { return v < 1 ? 1 : v > 0x7fffffff ? 0x7fffffff : int(v); }
}

EncounterError load_encounter(const Pack& pack, int32_t mission_id, Rng& rng,
                              std::vector<Wave>& out, int32_t* bad_id) {
    out.clear();
    auto fail = [&](EncounterError e, int32_t id) { out.clear(); if (bad_id) *bad_id = id; return e; };

    Table phases = pack.table("F_MISSION_PHASE_MST"), lottery = pack.table("F_BATTLE_LOTTERY_MST");
    Table groups = pack.table("F_BATTLE_GROUP_MST"), parts = pack.table("F_MONSTER_PARTS_MST");
    if (!phases.valid() || !lottery.valid() || !groups.valid() || !parts.valid())
        return fail(EncounterError::MissingTable, 0);

    Cols pc = cols(phases, { "mission_id", "phase_order", "phase_type", "lottery_id" });
    Cols lc = cols(lottery, { "lottery_id", "weight", "group_id" });
    Cols gc = cols(groups, { "group_id", "monster_id", "slot_order" });
    Cols mc = cols(parts, { "monster_id", "part_num", "hp", "mp", "atk", "def", "mag", "spr", "level" });
    int name_col = parts.column("name");   // optional
    int set_col = parts.column("skill_set_id");   // optional
    // Key columns must be column 0 (the pack sorts and searches on it).
    if (!pc.ok || !lc.ok || !gc.ok || !mc.ok || pc.v[0] || lc.v[0] || gc.v[0] || mc.v[0])
        return fail(EncounterError::MissingColumn, 0);

    for (uint32_t r = phases.lower_bound(mission_id); r < phases.rows() && phases.i(r, 0) == mission_id; ++r) {
        if (phases.i64(r, pc.v[2]) != kBattlePhase) continue;
        Wave w;
        w.phase = int(phases.i64(r, pc.v[1]));
        int32_t lot = int32_t(phases.i64(r, pc.v[3]));

        // Weighted pick among lottery members.
        uint32_t first = lottery.lower_bound(lot), end = first;
        int64_t total = 0;
        while (end < lottery.rows() && lottery.i(end, 0) == lot) { total += std::max<int64_t>(0, lottery.i64(end, lc.v[1])); ++end; }
        if (first == end) return fail(EncounterError::NoLottery, lot);
        uint32_t pick = first;
        if (total > 0) {
            int64_t roll = int64_t(rng.next()) % total;
            for (uint32_t i = first; i < end; ++i) {
                roll -= std::max<int64_t>(0, lottery.i64(i, lc.v[1]));
                if (roll < 0) { pick = i; break; }
            }
        }
        w.group_id = int32_t(lottery.i64(pick, lc.v[2]));

        uint32_t g = groups.lower_bound(w.group_id);
        if (g >= groups.rows() || groups.i(g, 0) != w.group_id) return fail(EncounterError::NoGroup, w.group_id);
        for (; g < groups.rows() && groups.i(g, 0) == w.group_id; ++g) {
            EnemySpec e;
            e.monster_id = int32_t(groups.i64(g, gc.v[1]));
            e.order = int(groups.i64(g, gc.v[2]));
            uint32_t p = parts.lower_bound(e.monster_id), body = parts.rows();
            for (; p < parts.rows() && parts.i(p, 0) == e.monster_id; ++p) {
                ++e.parts;
                if (body == parts.rows() || parts.i64(p, mc.v[1]) == 1) body = p;
            }
            if (!e.parts) return fail(EncounterError::NoMonsterStats, e.monster_id);
            e.hp = std::max<int64_t>(1, parts.i64(body, mc.v[2]));
            e.mp = int(std::min<int64_t>(0x7fffffff, std::max<int64_t>(0, parts.i64(body, mc.v[3]))));
            e.atk = clamp_int(parts.i64(body, mc.v[4]));
            e.def = clamp_int(parts.i64(body, mc.v[5]));
            e.mag = clamp_int(parts.i64(body, mc.v[6]));
            e.spr = clamp_int(parts.i64(body, mc.v[7]));
            e.level = clamp_int(parts.i64(body, mc.v[8]));
            if (set_col >= 0) e.skill_set_id = int32_t(parts.i64(body, uint32_t(set_col)));
            if (name_col >= 0 && parts.is_string(uint32_t(name_col))) e.name = parts.s(body, uint32_t(name_col));
            w.enemies.push_back(e);
        }
        std::stable_sort(w.enemies.begin(), w.enemies.end(),
                         [](const EnemySpec& a, const EnemySpec& b) { return a.order < b.order; });
        out.push_back(std::move(w));
    }
    if (out.empty()) return fail(EncounterError::NoBattlePhases, mission_id);
    std::stable_sort(out.begin(), out.end(), [](const Wave& a, const Wave& b) { return a.phase < b.phase; });
    return EncounterError::Ok;
}

void list_missions(const Pack& pack, std::vector<int32_t>& out) {
    out.clear();
    Table phases = pack.table("F_MISSION_PHASE_MST");
    if (!phases.valid()) return;
    int type = phases.column("phase_type");
    if (type < 0 || phases.column("mission_id") != 0) return;
    for (uint32_t r = 0; r < phases.rows(); ++r) {
        if (phases.i64(r, uint32_t(type)) != kBattlePhase) continue;
        int32_t id = phases.i(r, 0);
        if (out.empty() || out.back() != id) out.push_back(id);   // rows are sorted by mission id
    }
}

void spawn_wave(const Wave& wave, std::vector<Unit>& enemies) {
    enemies.clear();
    for (const EnemySpec& e : wave.enemies) {
        if (int(enemies.size()) >= kMaxPerSide) break;
        Unit u;
        u.id = uint32_t(e.monster_id); u.side = Side::Enemy; u.level = e.level;
        u.max_hp = u.hp = e.hp; u.max_mp = u.mp = e.mp;
        u.stat = { e.atk, e.def, e.mag, e.spr };
        enemies.push_back(u);
    }
}

} // namespace f3d
