// Host tests for the encounter chain. Fixture tables use the real table names
// and column keys (so the schema mapping is exercised) but invented contents.
#include "content/encounter.h"
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
