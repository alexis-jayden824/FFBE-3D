// Enemy skills from the converted master tables.
//
// Verified against real data (see docs/ASSETS.md): the skill set is a list of
// skill ids; each skill has a name, eight element flags in the order
// fire/ice/lightning/water/wind/earth/light/dark, an attack type
// (1 physical, 2 magic, 3 hybrid, 4 none), an effect class (1 = damage) and a
// hit timeline "frame:weight:?:?-frame:weight:?:?...".
//
// NOT decoded yet: power, targeting, costs, status effects, and the AI that
// decides which skill is used when. Loaded skills therefore all hit one target
// at 1.0x power, and only damage skills are loaded.
#pragma once
#include <cstdint>
#include <vector>

#include "content/pack.h"
#include "core/battle.h"

namespace f3d {

struct SkillInfo {
    uint32_t id = 0;
    const char* name = "";   // points into the pack
};

struct SkillLoadStats { int listed = 0, loaded = 0, skipped_non_damage = 0, missing = 0; };

// Appends the damage skills of `skill_set_id` to `abilities` (skipping ids
// already present) and their ids to `unit_abilities`. Names go to `names`.
// Returns false only if the tables or columns are absent.
bool load_monster_skills(const Pack& pack, int32_t skill_set_id,
                         std::vector<Ability>& abilities, std::vector<uint32_t>& unit_abilities,
                         std::vector<SkillInfo>& names, SkillLoadStats* stats = nullptr);

// Parses "f:w:a:b-f:w:a:b" (first '@'-separated alternative only) into the
// ability's hit timeline. Weights are normalised to sum to 1000 permille.
// Returns the number of hits (0 if the text has none; the ability is then left
// as a single hit at frame 0).
int parse_hit_frames(const char* text, Ability& ab);

} // namespace f3d
