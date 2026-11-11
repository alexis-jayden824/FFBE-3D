// Resolves a mission into its battle waves using the converted master tables:
//
//   mission phases -> (weighted) lottery -> battle group -> enemy slots -> monster part stats
//
// Column names are the readable names from tools/assets/schema.json. Each was
// checked against real data before being mapped; the ATK/DEF/MAG/SPR order of
// the four stat columns is inferred from position and still unverified.
#pragma once
#include <cstdint>
#include <vector>

#include "content/pack.h"
#include "core/battle.h"

namespace f3d {

struct EnemySpec {
    int32_t monster_id = 0;
    const char* name = "";  // points into the pack; valid while the pack blob is alive
    int order = 0;          // slot order within the group
    int parts = 0;          // number of part rows this monster has (only part 1 is used so far)
    int level = 1;
    int32_t skill_set_id = 0;   // 0 = this monster has no skill set
    int64_t hp = 1;
    int mp = 0, atk = 1, def = 1, mag = 1, spr = 1;
};

struct Wave {
    int phase = 0;          // phase order within the mission
    int32_t group_id = 0;
    std::vector<EnemySpec> enemies;
};

enum class EncounterError : uint8_t {
    Ok, MissingTable, MissingColumn, NoBattlePhases, NoLottery, NoGroup, NoMonsterStats
};
const char* to_string(EncounterError e);

// Fills `out` with one Wave per battle phase, in phase order. `rng` picks among
// weighted lottery members. On failure `out` is left empty and `bad_id` (if
// given) receives the id that could not be resolved.
EncounterError load_encounter(const Pack& pack, int32_t mission_id, Rng& rng,
                              std::vector<Wave>& out, int32_t* bad_id = nullptr);

// Every mission id that has at least one battle phase, ascending. Cheap: one
// pass over the phase table, no resolution (a listed mission may still fail to load).
void list_missions(const Pack& pack, std::vector<int32_t>& out);

// Turns one wave into enemy Units for the battle core.
void spawn_wave(const Wave& wave, std::vector<Unit>& enemies);

} // namespace f3d
