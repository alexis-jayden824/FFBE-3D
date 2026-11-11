// Host tests for the encounter chain. Fixture tables use the real table names
// and column keys (so the schema mapping is exercised) but invented contents.
#include "content/encounter.h"
#include "content/skills.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace f3d;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: test_encounter <pack>\n"); return 1; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Pack pack;
    CHECK(pack.open(blob.data(), blob.size()));

    // Range lookup over a multi-row key.
    Table groups = pack.table("F_BATTLE_GROUP_MST");
    uint32_t r = groups.lower_bound(50011);
    CHECK(r < groups.rows() && groups.i(r, 0) == 50011 && groups.i(r + 1, 0) == 50011);
    CHECK(groups.lower_bound(1) == 0 && groups.lower_bound(99999999) == groups.rows());

    // Mission 500: two battle phases (stored out of order) plus one non-battle phase.
    std::vector<Wave> waves; Rng rng(5); int32_t bad = 0;
    CHECK(load_encounter(pack, 500, rng, waves, &bad) == EncounterError::Ok);
    CHECK(waves.size() == 2);
    if (waves.size() == 2) {
        CHECK(waves[0].phase == 1 && waves[1].phase == 2);
        CHECK(waves[0].group_id == 50011 && waves[0].enemies.size() == 2);
        const EnemySpec& a = waves[0].enemies[0];   // slot order 1 comes first even though stored second
        CHECK(a.monster_id == 10 && a.order == 1 && a.hp == 90 && a.mp == 15);
        CHECK(a.atk == 30 && a.def == 14 && a.mag == 10 && a.spr == 13 && a.level == 1 && a.parts == 1);
        CHECK(waves[0].enemies[1].monster_id == 11 && waves[0].enemies[1].hp == 300);
        CHECK(waves[1].group_id == 50021 || waves[1].group_id == 50022);
    }

    // Weighted lottery: 75/25 split, both outcomes reachable, roughly in proportion.
    int heavy = 0, light = 0;
    for (uint32_t seed = 1; seed <= 2000; ++seed) {
        Rng s(seed); std::vector<Wave> w;
        if (load_encounter(pack, 500, s, w) != EncounterError::Ok || w.size() != 2) { ++g_fail; break; }
        (w[1].group_id == 50021 ? heavy : light)++;
    }
    CHECK(heavy + light == 2000);
    CHECK(heavy > 1350 && heavy < 1650);

    // Multi-part monster uses the part_num == 1 row; HP beyond 32 bits survives.
    for (uint32_t seed = 1; seed < 50; ++seed) {
        Rng s(seed); std::vector<Wave> w;
        load_encounter(pack, 500, s, w);
        const EnemySpec& e = w[1].enemies[0];
        if (e.monster_id == 12) { CHECK(e.parts == 2 && e.hp == 120 && e.atk == 22); }
        else { CHECK(e.monster_id == 13 && e.hp == 150000000000LL && e.level == 99); }
    }

    // Mission list: only missions with a battle phase, ascending, no duplicates.
    std::vector<int32_t> ids;
    list_missions(pack, ids);
    CHECK(ids.size() == 4 && ids[0] == 500 && ids[1] == 600 && ids[2] == 700 && ids[3] == 900);
    Pack none; list_missions(none, ids); CHECK(ids.empty());

    // Names come straight from the pack.
    { Rng s(1); std::vector<Wave> w; load_encounter(pack, 500, s, w);
      CHECK(std::string(w[0].enemies[0].name) == "Moss Imp" && std::string(w[0].enemies[1].name) == "Tin Sentry"); }

    // ---- enemy skills ----
    {
        Ability ab;
        CHECK(parse_hit_frames("40:33:2:2-52:33:2:2-64:34:2:2", ab) == 3);
        CHECK(ab.hit_count == 3 && ab.hit_frame[0] == 40 && ab.hit_frame[2] == 64);
        CHECK(ab.hit_weight_pm[0] == 330 && ab.hit_weight_pm[1] == 330 && ab.hit_weight_pm[2] == 340);
        CHECK(parse_hit_frames("10:50:2:2-30:50:2:2@99:100:2:2", ab) == 2 && ab.hit_frame[1] == 30 && ab.hit_weight_pm[1] == 500);
        CHECK(parse_hit_frames("", ab) == 0 && ab.hit_count == 1 && ab.hit_weight_pm[0] == 1000);
        CHECK(parse_hit_frames("garbage", ab) == 0 && ab.hit_count == 1);
        CHECK(parse_hit_frames("5:0:2:2", ab) == 0);                       // zero total weight
        std::string many;
        for (int i = 0; i < 50; ++i) many += (i ? "-" : "") + std::to_string(i * 3) + ":2:2:2";
        CHECK(parse_hit_frames(many.c_str(), ab) == kMaxHits);              // clamped, never overflows
        int sum = 0; for (int i = 0; i < ab.hit_count; ++i) sum += ab.hit_weight_pm[i];
        CHECK(sum == 1000);

        std::vector<Ability> abs; std::vector<uint32_t> mine; std::vector<SkillInfo> names; SkillLoadStats st;
        CHECK(load_monster_skills(pack, 810, abs, mine, names, &st));
        CHECK(st.listed == 3 && st.loaded == 2 && st.skipped_non_damage == 1 && st.missing == 0);
        CHECK(abs.size() == 2 && mine.size() == 2 && names.size() == 2);
        CHECK(abs[0].id == 9001 && abs[0].kind == AbilityKind::Physical && abs[0].element_mask == 0 && abs[0].hit_frame[0] == 42);
        CHECK(abs[1].id == 9002 && abs[1].kind == AbilityKind::Magic && abs[1].hit_count == 3);
        CHECK(abs[1].element_mask == ((1u << Fire) | (1u << Dark)));
        CHECK(std::string(names[1].name) == "Ember Spit");
        // Loading the same set again adds nothing new to the shared ability list.
        std::vector<uint32_t> again;
        CHECK(load_monster_skills(pack, 810, abs, again, names) && abs.size() == 2 && again.size() == 2 && names.size() == 2);
        // Set 812 lists one hybrid water skill and one id that does not exist.
        SkillLoadStats st2; std::vector<uint32_t> cube;
        CHECK(load_monster_skills(pack, 812, abs, cube, names, &st2) && st2.loaded == 1 && st2.missing == 1);
        CHECK(abs.back().kind == AbilityKind::Hybrid && abs.back().element_mask == (1u << Water) && abs.back().hit_count == 2);
        // No set (0) and unknown set are not errors; they just add nothing.
        std::vector<uint32_t> nothing; SkillLoadStats st3;
        CHECK(load_monster_skills(pack, 0, abs, nothing, names, &st3) && nothing.empty() && st3.missing == 0);
        CHECK(load_monster_skills(pack, 777, abs, nothing, names, &st3) && nothing.empty() && st3.missing == 1);
        Pack none2; CHECK(!load_monster_skills(none2, 810, abs, nothing, names));

        // The encounter carries each monster's skill set id.
        Rng s(1); std::vector<Wave> w; load_encounter(pack, 500, s, w);
        CHECK(w[0].enemies[0].skill_set_id == 810 && w[0].enemies[1].skill_set_id == 0);

        // An enemy with several skills uses more than one of them over time, and events say which.
        Battle b(11);
        b.abilities = abs;
        Unit hero; hero.max_hp = hero.hp = 1000000; hero.stat = { 10, 50, 10, 50 };
        b.party = { hero };
        Unit imp; imp.side = Side::Enemy; imp.max_hp = imp.hp = 100; imp.stat = { 30, 10, 30, 10 }; imp.abilities = mine;
        b.enemies = { imp };
        std::vector<HitEvent> ev; bool saw1 = false, saw2 = false;
        for (int t = 0; t < 40; ++t) { b.enemy_turn(ev); b.end_round(); }
        for (const HitEvent& e : ev) { saw1 |= e.ability == 9001; saw2 |= e.ability == 9002; CHECK(e.ability == 9001 || e.ability == 9002); }
        CHECK(saw1 && saw2);
    }

    // Same seed, same result.
    Rng r1(77), r2(77); std::vector<Wave> w1, w2;
    load_encounter(pack, 500, r1, w1); load_encounter(pack, 500, r2, w2);
    CHECK(w1.size() == w2.size() && w1[1].group_id == w2[1].group_id);

    // Broken references are reported with the offending id, never a crash.
    CHECK(load_encounter(pack, 600, rng, waves, &bad) == EncounterError::NoLottery && bad == 9999 && waves.empty());
    CHECK(load_encounter(pack, 700, rng, waves, &bad) == EncounterError::NoGroup && bad == 70011);
    CHECK(load_encounter(pack, 900, rng, waves, &bad) == EncounterError::NoMonsterStats && bad == 404);
    CHECK(load_encounter(pack, 800, rng, waves, &bad) == EncounterError::NoBattlePhases && bad == 800);
    CHECK(load_encounter(pack, 12345, rng, waves, &bad) == EncounterError::NoBattlePhases);
    Pack empty;
    CHECK(load_encounter(empty, 500, rng, waves) == EncounterError::MissingTable);

    // A loaded wave can actually be fought and won by the core.
    Rng r3(9);
    CHECK(load_encounter(pack, 500, r3, waves) == EncounterError::Ok);
    Battle b(3);
    Ability hit; hit.id = 1; hit.hit_count = 1; hit.hit_weight_pm[0] = 1000;
    b.abilities = { hit };
    Unit hero; hero.max_hp = hero.hp = 5000; hero.stat = { 200, 100, 50, 50 }; hero.abilities = { 1 };
    b.party = { hero, hero };
    spawn_wave(waves[0], b.enemies);
    CHECK(b.enemies.size() == 2 && b.enemies[0].hp == 90 && b.enemies[1].stat[DEF] == 60);
    std::vector<HitEvent> ev;
    for (int t = 0; t < 20 && b.outcome() == Outcome::Ongoing; ++t) {
        uint8_t tgt = b.enemies[0].alive() ? 0 : 1;
        b.resolve(Side::Party, { { 0, 1, tgt, 0 }, { 1, 1, tgt, 0 } }, ev);
        b.end_round();
    }
    CHECK(b.outcome() == Outcome::Victory);

    std::printf(g_fail ? "%d check(s) failed\n" : "all encounter tests passed\n", g_fail);
    return g_fail ? 1 : 0;
}
