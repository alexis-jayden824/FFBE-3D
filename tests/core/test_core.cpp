// Host tests for the portable core. All units and abilities here are original
// placeholders; no game data is needed to run them.
#include "core/battle.h"
#include "content/pack.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

using namespace f3d;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static Unit mk(Side side, int hp, int atk, int def, int mag = 10, int spr = 10) {
    Unit u; u.side = side; u.level = 0; u.max_hp = u.hp = hp; u.max_mp = u.mp = 100;
    u.stat = { atk, def, mag, spr };
    return u;
}

static Ability strike(uint32_t id, int hits, int gap) {
    Ability a; a.id = id; a.kind = AbilityKind::Physical; a.hit_count = uint8_t(hits);
    for (int i = 0; i < hits; ++i) { a.hit_frame[i] = uint16_t(i * gap); a.hit_weight_pm[i] = uint16_t(1000 / hits); }
    return a;
}

static int total(const std::vector<HitEvent>& ev) { int s = 0; for (auto& e : ev) s += e.damage; return s; }

static Battle two_vs_one(uint32_t seed = 7) {
    Battle b(seed);
    b.abilities = { strike(1, 4, 10) };
    for (int i = 0; i < 2; ++i) { b.party.push_back(mk(Side::Party, 500, 100, 50)); b.party.back().abilities = { 1 }; }
    b.enemies.push_back(mk(Side::Enemy, 1000000, 40, 50));
    b.enemies.back().abilities = { 1 };
    return b;
}

static void test_damage_formula() {
    Battle b(1);
    b.abilities = { strike(1, 1, 0) };
    b.party.push_back(mk(Side::Party, 100, 100, 10)); b.party[0].abilities = { 1 };
    b.enemies.push_back(mk(Side::Enemy, 100000, 10, 50));
    std::vector<HitEvent> ev;
    b.resolve(Side::Party, { { 0, 1, 0, 0 } }, ev);
    // 100^2 / 50 = 200, variance 85-100%
    CHECK(ev.size() == 1);
    CHECK(ev[0].damage >= 170 && ev[0].damage <= 200);
    CHECK(ev[0].chain_pm == 1000);
}

static void test_chain_beats_no_chain() {
    // Same seed, same actions; only the second unit's timing differs.
    Battle a = two_vs_one(), b = two_vs_one();
    std::vector<HitEvent> ea, eb;
    a.resolve(Side::Party, { { 0, 1, 0, 0 }, { 1, 1, 0, 5 } }, ea);     // interleaved: chains
    b.resolve(Side::Party, { { 0, 1, 0, 0 }, { 1, 1, 0, 200 } }, eb);   // far apart: no chain
    CHECK(ea.size() == 8 && eb.size() == 8);
    CHECK(total(ea) > total(eb));
    CHECK(ea.back().chain_count == 8);
    for (auto& e : eb) CHECK(e.chain_pm == 1000);   // one unit alone never chains with itself
    for (size_t i = 1; i < ea.size(); ++i) CHECK(ea[i].frame >= ea[i - 1].frame);
}

static void test_spark_and_cap() {
    Battle b = two_vs_one();
    b.abilities = { strike(1, 30, 4) };
    std::vector<HitEvent> ev;
    b.resolve(Side::Party, { { 0, 1, 0, 0 }, { 1, 1, 0, 0 } }, ev);   // identical timing: every pair sparks
    bool any_spark = false; int peak = 0;
    for (auto& e : ev) { any_spark |= e.spark; if (e.chain_pm > peak) peak = e.chain_pm; CHECK(e.chain_pm <= b.chain.cap_pm); }
    CHECK(any_spark);
    CHECK(peak == b.chain.cap_pm);
}

static void test_element_and_guard() {
    Battle b = two_vs_one();
    b.abilities[0].element_mask = 1u << Fire;
    Battle weak = b, immune = b, guard = b;
    weak.enemies[0].resist_pm[Fire] = -1000;
    immune.enemies[0].resist_pm[Fire] = 1000;
    guard.enemies[0].guarding = true;
    std::vector<HitEvent> e0, e1, e2, e3;
    std::vector<Action> act = { { 0, 1, 0, 0 } };
    b.resolve(Side::Party, act, e0); weak.resolve(Side::Party, act, e1);
    immune.resolve(Side::Party, act, e2); guard.resolve(Side::Party, act, e3);
    CHECK(total(e1) > total(e0) * 19 / 10);
    CHECK(total(e2) <= 4);                       // floor of 1 per hit
    CHECK(total(e3) < total(e0) * 6 / 10);
}

static void test_legality_mp_buffs() {
    Battle b = two_vs_one();
    Ability costly = strike(2, 1, 0); costly.mp_cost = 80;
    Ability brave; brave.id = 3; brave.kind = AbilityKind::Buff; brave.target = Target::AllAllies;
    brave.buff_stat = ATK; brave.buff_pm = 500; brave.buff_turns = 2;
    b.abilities.push_back(costly); b.abilities.push_back(brave);
    b.party[0].abilities = { 1, 2, 3 };

    CHECK(!b.can_use(Side::Party, { 1, 2, 0, 0 }));    // unit 1 does not know ability 2
    CHECK(!b.can_use(Side::Party, { 0, 99, 0, 0 }));   // unknown ability
    CHECK(!b.can_use(Side::Party, { 0, 1, 5, 0 }));    // target out of range
    std::vector<HitEvent> ev;
    b.resolve(Side::Party, { { 0, 2, 0, 0 }, { 0, 2, 0, 30 } }, ev);   // second cast lacks MP
    CHECK(ev.size() == 1);
    CHECK(b.party[0].mp == 20);

    b.resolve(Side::Party, { { 0, 3, 0, 0 } }, ev);
    CHECK(b.party[0].eff(ATK) == 150 && b.party[1].eff(ATK) == 150);
    b.end_round(); CHECK(b.party[0].eff(ATK) == 150);
    b.end_round(); CHECK(b.party[0].eff(ATK) == 100);
    CHECK(b.turn == 3);
}

static void test_determinism_and_outcome() {
    auto run = [](uint32_t seed) {
        Battle b = two_vs_one(seed);
        b.enemies[0].max_hp = b.enemies[0].hp = 3000;
        std::vector<HitEvent> ev;
        while (b.outcome() == Outcome::Ongoing && b.turn < 200) {
            b.resolve(Side::Party, { { 0, 1, 0, 0 }, { 1, 1, 0, 5 } }, ev);
            if (b.outcome() != Outcome::Ongoing) break;
            b.enemy_turn(ev);
            b.end_round();
        }
        CHECK(b.outcome() == Outcome::Victory);
        CHECK(b.enemies[0].hp == 0);
        return ev;
    };
    auto a = run(42), b = run(42), c = run(43);
    CHECK(a.size() == b.size());
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); ++i) same = a[i].damage == b[i].damage && a[i].target == b[i].target;
    CHECK(same);
    bool differs = a.size() != c.size();
    for (size_t i = 0; !differs && i < a.size(); ++i) differs = a[i].damage != c[i].damage;
    CHECK(differs);

    Battle lose(1);
    lose.abilities = { strike(1, 1, 0) };
    lose.party.push_back(mk(Side::Party, 1, 1, 1));
    lose.enemies.push_back(mk(Side::Enemy, 100, 100, 1)); lose.enemies[0].abilities = { 1 };
    std::vector<HitEvent> ev;
    lose.enemy_turn(ev);
    CHECK(lose.outcome() == Outcome::Defeat);
    CHECK(!ev.empty() && ev.back().killed);
}

static void test_pack(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Pack p;
    CHECK(p.open(blob.data(), blob.size()));
    CHECK(p.table_count() == 1);
    Table t = p.table("T_SAMPLE");
    CHECK(t.valid() && t.rows() == 3 && t.cols() == 4);
    CHECK(!p.table("NOPE").valid());
    int name = t.column("name"), hp = t.column("hp"), tags = t.column("tags");
    CHECK(name == 1 && hp == 2 && tags == 3 && t.column("zz") == -1);
    CHECK(!t.is_string(0) && t.is_string(1) && !t.is_string(2) && t.is_string(3));
    CHECK(t.i(0, 0) == 10 && t.i(1, 0) == 20 && t.i(2, 0) == 30);   // sorted by id
    int r = t.find(20);
    CHECK(r == 1 && std::strcmp(t.s(r, name), "Tin Sentry") == 0 && t.i(r, hp) == 300);
    CHECK(std::strcmp(t.s(t.find(10), tags), "") == 0);
    CHECK(t.find(15) == -1 && t.find(-5) == -1 && t.find(999) == -1);

    // Corrupt inputs must be rejected, not crash.
    Pack bad;
    CHECK(!bad.open(nullptr, 0));
    CHECK(!bad.open(blob.data(), 8));
    std::vector<uint8_t> trunc(blob.begin(), blob.begin() + blob.size() / 2);
    CHECK(!bad.open(trunc.data(), trunc.size()));
    std::vector<uint8_t> magic = blob; magic[0] = 'X';
    CHECK(!bad.open(magic.data(), magic.size()));
}

int main(int argc, char** argv) {
    test_damage_formula();
    test_chain_beats_no_chain();
    test_spark_and_cap();
    test_element_and_guard();
    test_legality_mp_buffs();
    test_determinism_and_outcome();
    if (argc > 1) test_pack(argv[1]); else { std::printf("FAIL: no pack fixture path given\n"); ++g_fail; }
    std::printf(g_fail ? "%d check(s) failed\n" : "all core tests passed\n", g_fail);
    return g_fail ? 1 : 0;
}
