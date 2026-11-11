// PC-side inspector for converted packs.
//   f3d_inspect <pack> tables              list tables
//   f3d_inspect <pack> mission <id>        print the waves a mission resolves to
//   f3d_inspect <pack> validate            count how many missions resolve end to end
#include "content/encounter.h"
#include "content/skills.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

using namespace f3d;

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: f3d_inspect <pack> tables | mission <id> | validate\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Pack pack;
    if (!pack.open(blob.data(), blob.size())) { std::fprintf(stderr, "%s: not a valid .f3dp pack\n", argv[1]); return 1; }

    if (!std::strcmp(argv[2], "tables")) {
        for (uint32_t i = 0; i < pack.table_count(); ++i) {
            Table t = pack.table(i);
            std::printf("%-28s %7u rows %3u cols\n", t.name(), t.rows(), t.cols());
        }
        return 0;
    }
    if (!std::strcmp(argv[2], "mission") && argc > 3) {
        Rng rng(1); std::vector<Wave> waves; int32_t bad = 0;
        EncounterError e = load_encounter(pack, std::atoi(argv[3]), rng, waves, &bad);
        if (e != EncounterError::Ok) { std::printf("mission %s: %s (id %d)\n", argv[3], to_string(e), bad); return 1; }
        for (const Wave& w : waves) {
            std::printf("wave %d  group %d\n", w.phase, w.group_id);
            for (const EnemySpec& m : w.enemies)
                std::printf("  slot %2d  monster %-10d lv %-3d hp %-12lld mp %-8d atk %-8d def %-10d mag %-8d spr %-10d parts %d\n",
                            m.order, m.monster_id, m.level, (long long)m.hp, m.mp, m.atk, m.def, m.mag, m.spr, m.parts);
            std::vector<Ability> abs; std::vector<SkillInfo> names;
            for (const EnemySpec& m : w.enemies) { std::vector<uint32_t> mine; load_monster_skills(pack, m.skill_set_id, abs, mine, names); }
            for (size_t i = 0; i < abs.size(); ++i)
                std::printf("  skill %-8u %-28s kind %d elements 0x%02x hits %d (first at frame %d)\n", abs[i].id, names[i].name,
                            int(abs[i].kind), abs[i].element_mask, abs[i].hit_count, abs[i].hit_frame[0]);
        }
        return 0;
    }
    if (!std::strcmp(argv[2], "validate")) {
        Table phases = pack.table("F_MISSION_PHASE_MST");
        if (!phases.valid()) { std::fprintf(stderr, "no F_MISSION_PHASE_MST\n"); return 1; }
        std::map<EncounterError, int> tally; int missions = 0, waves_total = 0; size_t max_enemies = 0;
        int32_t prev = 0; bool have = false;
        for (uint32_t r = 0; r < phases.rows(); ++r) {
            int32_t id = phases.i(r, 0);
            if (have && id == prev) continue;
            prev = id; have = true; ++missions;
            Rng rng(1); std::vector<Wave> waves;
            EncounterError e = load_encounter(pack, id, rng, waves);
            ++tally[e];
            waves_total += int(waves.size());
            for (const Wave& w : waves) if (w.enemies.size() > max_enemies) max_enemies = w.enemies.size();
        }
        std::printf("missions in phase table: %d\n", missions);
        for (auto& kv : tally) std::printf("  %6d  %s\n", kv.second, to_string(kv.first));
        std::printf("waves resolved: %d   largest wave: %zu enemies\n", waves_total, max_enemies);
        Table parts = pack.table("F_MONSTER_PARTS_MST");
        int set_col = parts.valid() ? parts.column("skill_set_id") : -1;
        if (set_col >= 0) {
            SkillLoadStats st; int with = 0, without = 0, no_damage = 0; bool tables = true;
            for (uint32_t r = 0; r < parts.rows() && tables; ++r) {
                int32_t id = int32_t(parts.i64(r, uint32_t(set_col)));
                if (!id) { ++without; continue; }
                std::vector<Ability> abs; std::vector<uint32_t> mine; std::vector<SkillInfo> names;
                tables = load_monster_skills(pack, id, abs, mine, names, &st);
                ++with; if (mine.empty()) ++no_damage;
            }
            if (!tables) std::printf("skill tables not in this pack\n");
            else std::printf("monster parts: %d with a skill set, %d without; %d of those with a set have no damage skill\n"
                             "skills listed: %d   loaded as damage: %d   non-damage (not implemented): %d   missing: %d\n",
                             with, without, no_damage, st.listed, st.loaded, st.skipped_non_damage, st.missing);
        }
        return 0;
    }
    std::fprintf(stderr, "unknown command\n");
    return 2;
}
