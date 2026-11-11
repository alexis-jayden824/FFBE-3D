#include "content/units.h"
#include <cstdlib>

#include "content/skills.h"

namespace f3d {

const char* to_string(UnitError e) {
    switch (e) {
        case UnitError::Ok: return "ok";
        case UnitError::MissingTable: return "unit table is missing from the pack";
        case UnitError::MissingColumn: return "a required unit column is missing (schema out of date?)";
        case UnitError::NotFound: return "unit id not in the unit table";
    }
    return "?";
}

namespace {
// Second number of "a,b,c,d" (the max-level value), clamped to at least `floor`.
int max_level_value(const char* text, int floor) {
    while (*text && *text != ',') ++text;
    if (*text != ',') return floor;
    long v = std::strtol(text + 1, nullptr, 10);
    if (v < floor) v = floor;
    if (v > 0x7fffffff) v = 0x7fffffff;
    return int(v);
}
}

UnitError load_unit(const Pack& pack, int32_t id, UnitSpec& out) {
    out = UnitSpec();
    Table t = pack.table("F_UNIT_MST");
    if (!t.valid()) return UnitError::MissingTable;
    static const char* const names[] = { "name_jp", "rarity", "max_level", "base_unit_id", "hp", "mp", "atk", "def", "mag", "spr", "attack_frames" };
    int c[11];
    for (int i = 0; i < 11; ++i) { c[i] = t.column(names[i]); if (c[i] < 0) return UnitError::MissingColumn; }
    if (t.column("unit_id") != 0) return UnitError::MissingColumn;
    for (int i : { 0, 4, 5, 6, 7, 8, 9, 10 }) if (!t.is_string(uint32_t(c[i]))) return UnitError::MissingColumn;

    // Exact id, else the nearest lower rarity form (ids of one unit differ only in the last digit).
    int row = -1;
    int32_t found = id;
    for (int32_t probe = id; probe > 0 && probe / 10 == id / 10; --probe) {
        row = t.find(probe);
        if (row >= 0) { found = probe; break; }
    }
    if (row < 0) return UnitError::NotFound;
    uint32_t r = uint32_t(row);

    out.id = id;
    out.stats_from = found;
    out.rarity = int(t.i64(r, uint32_t(c[1])));
    long lvl = long(t.i64(r, uint32_t(c[2])));
    out.max_level = lvl < 1 ? 1 : lvl > 999 ? 999 : int(lvl);
    out.hp = max_level_value(t.s(r, uint32_t(c[4])), 1);
    out.mp = max_level_value(t.s(r, uint32_t(c[5])), 0);
    out.atk = max_level_value(t.s(r, uint32_t(c[6])), 1);
    out.def = max_level_value(t.s(r, uint32_t(c[7])), 1);
    out.mag = max_level_value(t.s(r, uint32_t(c[8])), 1);
    out.spr = max_level_value(t.s(r, uint32_t(c[9])), 1);
    out.attack_frames = t.s(r, uint32_t(c[10]));
    out.name = t.s(r, uint32_t(c[0]));

    // English name: by the id asked for, the row used, then the unit's base id.
    Table text = pack.table("F_TEXT_UNITS_NAME");
    int tc = text.valid() ? text.column("text") : -1;
    if (tc >= 0 && text.column("id") == 0 && text.is_string(uint32_t(tc))) {
        const int32_t tries[] = { id, found, int32_t(t.i64(r, uint32_t(c[3]))) };
        for (int32_t k : tries) {
            int nr = text.find(k);
            if (nr >= 0 && *text.s(uint32_t(nr), uint32_t(tc))) { out.name = text.s(uint32_t(nr), uint32_t(tc)); break; }
        }
    }
    return UnitError::Ok;
}

void make_party_unit(const UnitSpec& spec, uint32_t attack_ability_id, Unit& unit, Ability& attack) {
    unit = Unit();
    unit.id = uint32_t(spec.id);
    unit.side = Side::Party;
    unit.level = spec.max_level;
    unit.max_hp = unit.hp = spec.hp;
    unit.max_mp = unit.mp = spec.mp;
    unit.stat = { spec.atk, spec.def, spec.mag, spec.spr };
    unit.abilities = { attack_ability_id };

    attack = Ability();
    attack.id = attack_ability_id;
    attack.kind = AbilityKind::Physical;
    attack.target = Target::OneEnemy;
    parse_hit_frames(spec.attack_frames, attack);
}

} // namespace f3d
