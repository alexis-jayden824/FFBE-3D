#include "content/skills.h"
#include <cstdlib>

namespace f3d {

int parse_hit_frames(const char* text, Ability& ab) {
    int frames[kMaxHits], weights[kMaxHits], n = 0;
    const char* p = text;
    while (*p && *p != '@' && n < kMaxHits) {
        char* end = nullptr;
        long f = std::strtol(p, &end, 10);
        if (end == p || *end != ':') break;
        p = end + 1;
        long w = std::strtol(p, &end, 10);
        if (end == p) break;
        frames[n] = f < 0 ? 0 : f > 0xffff ? 0xffff : int(f);
        weights[n] = w < 0 ? 0 : int(w);
        ++n;
        p = end;
        while (*p && *p != '-' && *p != '@') ++p;   // skip the two trailing fields
        if (*p == '-') ++p;
    }
    int total = 0;
    for (int i = 0; i < n; ++i) total += weights[i];
    if (n == 0 || total <= 0) {
        ab.hit_count = 1; ab.hit_frame[0] = 0; ab.hit_weight_pm[0] = 1000;
        return 0;
    }
    int used = 0;
    for (int i = 0; i < n; ++i) {
        int pm = (i == n - 1) ? 1000 - used : weights[i] * 1000 / total;
        used += pm;
        ab.hit_frame[i] = uint16_t(frames[i]);
        ab.hit_weight_pm[i] = uint16_t(pm);
    }
    ab.hit_count = uint8_t(n);
    return n;
}

bool load_monster_skills(const Pack& pack, int32_t skill_set_id,
                         std::vector<Ability>& abilities, std::vector<uint32_t>& unit_abilities,
                         std::vector<SkillInfo>& names, SkillLoadStats* stats) {
    Table sets = pack.table("F_MONSTER_SKILL_SET_MST"), skills = pack.table("F_MONSTER_SKILL_MST");
    if (!sets.valid() || !skills.valid()) return false;
    int ids = sets.column("skill_ids");
    int name = skills.column("name"), elem = skills.column("element_flags"), type = skills.column("attack_type");
    int cls = skills.column("effect_class"), frames = skills.column("hit_frames");
    if (sets.column("skill_set_id") != 0 || skills.column("skill_id") != 0 ||
        ids < 0 || name < 0 || elem < 0 || type < 0 || cls < 0 || frames < 0) return false;
    if (!sets.is_string(uint32_t(ids)) || !skills.is_string(uint32_t(name)) ||
        !skills.is_string(uint32_t(elem)) || !skills.is_string(uint32_t(frames))) return false;

    if (skill_set_id == 0) return true;   // "no skill set" sentinel
    int row = sets.find(skill_set_id);
    if (row < 0) { if (stats) ++stats->missing; return true; }

    const char* p = sets.s(uint32_t(row), uint32_t(ids));
    while (*p) {
        char* end = nullptr;
        long id = std::strtol(p, &end, 10);
        if (end != p && id > 0) {
            if (stats) ++stats->listed;
            int r = skills.find(int32_t(id));
            if (r < 0) { if (stats) ++stats->missing; }
            else {
                long t = long(skills.i64(uint32_t(r), uint32_t(type)));
                if (skills.i64(uint32_t(r), uint32_t(cls)) != 1 || t < 1 || t > 3) {
                    if (stats) ++stats->skipped_non_damage;
                } else {
                    bool have = false;
                    for (const Ability& a : abilities) if (a.id == uint32_t(id)) { have = true; break; }
                    if (!have) {
                        Ability ab;
                        ab.id = uint32_t(id);
                        ab.kind = t == 1 ? AbilityKind::Physical : t == 2 ? AbilityKind::Magic : AbilityKind::Hybrid;
                        ab.target = Target::OneEnemy;
                        int bit = 0;
                        for (const char* e = skills.s(uint32_t(r), uint32_t(elem)); *e && bit < ElementCount; ++e) {
                            if (*e == ',') continue;
                            if (*e == '1') ab.element_mask |= uint8_t(1u << bit);
                            ++bit;
                        }
                        parse_hit_frames(skills.s(uint32_t(r), uint32_t(frames)), ab);
                        abilities.push_back(ab);
                        names.push_back({ ab.id, skills.s(uint32_t(r), uint32_t(name)) });
                    }
                    bool listed = false;
                    for (uint32_t u : unit_abilities) if (u == uint32_t(id)) { listed = true; break; }
                    if (!listed) unit_abilities.push_back(uint32_t(id));
                    if (stats) ++stats->loaded;
                }
            }
        }
        p = (end != p) ? end : p;
        while (*p && *p != ',') ++p;
        if (*p == ',') ++p;
    }
    return true;
}

} // namespace f3d
