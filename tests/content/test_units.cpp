// Host tests for player unit loading. Fixture units are invented; table and
// column keys are the real ones so the schema mapping is exercised.
#include "content/units.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace f3d;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: test_units <pack>\n"); return 1; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Pack pack;
    CHECK(pack.open(blob.data(), blob.size()));
    CHECK(pack.table("F_UNIT_MST").column("zzIGNORED") == -1);          // unmapped columns are dropped by the preset
    CHECK(pack.table("F_TEXT_UNITS_NAME").rows() == 2);                 // the key without a number is skipped

    UnitSpec u;
    CHECK(load_unit(pack, 700000105, u) == UnitError::Ok);
    CHECK(u.id == 700000105 && u.stats_from == 700000105 && u.rarity == 5 && u.max_level == 80);
    CHECK(u.hp == 2500 && u.mp == 95 && u.atk == 90 && u.def == 88 && u.mag == 86 && u.spr == 80);   // max-level values
    CHECK(std::string(u.name) == "Kestrel");                            // English name via the base unit id

    // A form with no row of its own uses the nearest lower form and says so.
    CHECK(load_unit(pack, 700000107, u) == UnitError::Ok);
    CHECK(u.id == 700000107 && u.stats_from == 700000105 && u.hp == 2500);
    CHECK(load_unit(pack, 700000104, u) == UnitError::Ok && u.stats_from == 700000103 && u.hp == 400 && u.max_level == 40);
    CHECK(std::string(u.name) == "Kestrel");

    // No English text: falls back to the table's own name. Broken cells never produce zero or negative stats.
    CHECK(load_unit(pack, 700000205, u) == UnitError::Ok);
    CHECK(std::string(u.name) == "Only JP" && u.hp == 1 && u.mp == 0 && u.atk == 1);

    CHECK(load_unit(pack, 700000102, u) == UnitError::NotFound);        // below the lowest form
    CHECK(load_unit(pack, 123, u) == UnitError::NotFound);
    Pack none;
    CHECK(load_unit(none, 700000105, u) == UnitError::MissingTable);

    // Battle unit and its normal attack.
    CHECK(load_unit(pack, 700000105, u) == UnitError::Ok);
    Unit unit; Ability atk;
    make_party_unit(u, 0xF0000001u, unit, atk);
    CHECK(unit.side == Side::Party && unit.level == 80 && unit.hp == 2500 && unit.max_hp == 2500 && unit.mp == 95);
    CHECK(unit.stat[ATK] == 90 && unit.stat[DEF] == 88 && unit.stat[MAG] == 86 && unit.stat[SPR] == 80);
    CHECK(unit.abilities.size() == 1 && unit.abilities[0] == 0xF0000001u);
    CHECK(atk.id == 0xF0000001u && atk.kind == AbilityKind::Physical && atk.hit_count == 4);
    CHECK(atk.hit_frame[0] == 12 && atk.hit_frame[3] == 36 && atk.hit_weight_pm[0] == 250);

    // Two different units' attacks interleave into a chain in the core.
    UnitSpec a; load_unit(pack, 700000103, a);
    Battle b(5);
    Unit u2; Ability atk2;
    make_party_unit(a, 0xF0000002u, u2, atk2);
    CHECK(atk2.hit_count == 2 && atk2.hit_frame[1] == 36 && atk2.hit_weight_pm[0] == 600);
    b.party = { unit, u2 };
    b.abilities = { atk, atk2 };
    Unit dummy; dummy.side = Side::Enemy; dummy.max_hp = dummy.hp = 100000000; dummy.stat = { 1, 50, 1, 50 };
    b.enemies = { dummy };
    std::vector<HitEvent> ev;
    b.resolve(Side::Party, { { 0, 0xF0000001u, 0, 0 }, { 1, 0xF0000002u, 0, 0 } }, ev);
    CHECK(ev.size() == 6);
    int best = 0; for (const HitEvent& e : ev) if (e.chain_count > best) best = e.chain_count;
    CHECK(best == 3);   // hits at 12(A) 15(B) 20(A) link; A's own next hits at 28 and 36 do not chain with itself

    std::printf(g_fail ? "%d check(s) failed\n" : "all unit tests passed\n", g_fail);
    return g_fail ? 1 : 0;
}
