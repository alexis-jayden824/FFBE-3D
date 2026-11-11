// Player units from the converted master tables (pack built with
// `mstpack.py --preset units`).
//
// Verified against real data (docs/ASSETS.md): F_UNIT_MST has one row per
// rarity form of a unit; each stat column is "level-1 value, max-level value,
// two further numbers" in the order HP, MP, ATK, DEF, MAG, SPR; attack_frames
// is the normal attack's hit timeline in the same format enemy skills use.
//
// Not used yet: the growth curve between level 1 and max level (units are
// created at max level), the two extra numbers per stat, equipment, passives,
// and every ability other than the normal attack.
#pragma once
#include <cstdint>

#include "content/pack.h"
#include "core/battle.h"

namespace f3d {

struct UnitSpec {
    int32_t id = 0;            // the id that was asked for
    int32_t stats_from = 0;    // the row the stats came from (differs when `id` has no row of its own)
    const char* name = "";     // English name if the text table has one, else the table's own name; points into the pack
    int rarity = 0, max_level = 1;
    int hp = 1, mp = 0, atk = 1, def = 1, mag = 1, spr = 1;   // at max level, no equipment
    const char* attack_frames = "";
};

enum class UnitError : uint8_t { Ok, MissingTable, MissingColumn, NotFound };
const char* to_string(UnitError e);

// Looks a unit up by id. Some sprite ids (mostly the highest rarity form of
// older units) have no row in the table; for those the nearest lower form of
// the same unit is used and reported in `stats_from`.
UnitError load_unit(const Pack& pack, int32_t id, UnitSpec& out);

// Builds the battle Unit (at max level) and its normal attack.
void make_party_unit(const UnitSpec& spec, uint32_t attack_ability_id, Unit& unit, Ability& attack);

} // namespace f3d
