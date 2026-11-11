// Console front end, Milestone 1/2 bridge.
//
// Scenes: mission select -> battle (all waves of the mission) -> result.
// Enemies and their damage skills (name, element, attack type, hit timing)
// come from the encounter pack on SD if present. Party members are drawn with
// real converted unit sprites if party/1.f3ds..6.f3ds exist on SD; their stats
// are still placeholders. Skill power, targeting and enemy AI are not decoded,
// and enemies and backgrounds are still placeholder rectangles.
// Without a pack the program still runs, with a built-in placeholder fight.
//
// What has and has not been run on hardware is tracked in docs/TEST_REPORT.md.
#include <3ds.h>
#include <citro2d.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "content/encounter.h"
#include "content/skills.h"
#include "core/battle.h"
#include "unit_sprite.h"

using namespace f3d;

namespace {

constexpr const char* kPackPath = "sdmc:/3ds/ffbe3d/encounter.f3dp";
constexpr int kParty = 6;
constexpr const char* kPartyDir = "sdmc:/3ds/ffbe3d/party";   // 1.f3ds .. 6.f3ds, one per party slot
constexpr float kUnitSize = 1.0f;                               // relative to the original art

constexpr u32 kBg = C2D_Color32(18, 20, 28, 255);
constexpr u32 kPanel = C2D_Color32(40, 46, 64, 255);
constexpr u32 kFocus = C2D_Color32(240, 200, 80, 255);
constexpr u32 kHp = C2D_Color32(80, 200, 120, 255);
constexpr u32 kHpBack = C2D_Color32(70, 30, 30, 255);
constexpr u32 kAlly = C2D_Color32(90, 140, 230, 255);
constexpr u32 kFoe = C2D_Color32(210, 90, 90, 255);
constexpr u32 kDim = C2D_Color32(90, 90, 100, 255);
constexpr u32 kWhite = C2D_Color32(255, 255, 255, 255);
constexpr u32 kShade = C2D_Color32(0, 0, 0, 170);

C2D_TextBuf g_text;

void text(float x, float y, float size, u32 color, const char* fmt, ...) {
    char buf[128];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    C2D_Text t;
    C2D_TextParse(&t, g_text, buf);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.6f, size, size, color);
}

void bar(float x, float y, float w, float h, int64_t cur, int64_t max) {
    C2D_DrawRectSolid(x, y, 0.4f, w, h, kHpBack);
    if (max > 0 && cur > 0) C2D_DrawRectSolid(x, y, 0.45f, w * float(double(cur) / double(max)), h, kHp);
}

struct Rect { float x, y, w, h; };
bool inside(const Rect& r, const touchPosition& t) { return t.px >= r.x && t.px < r.x + r.w && t.py >= r.y && t.py < r.y + r.h; }
Rect card(int i) { return { 4.f + (i % 2) * 158.f, 4.f + (i / 2) * 62.f, 154.f, 58.f }; }

// Enemy placement on the top screen: columns of up to 5, shrinking as the wave grows (max 25+).
Rect enemy_rect(int i, int n) {
    int cols = (n + 4) / 5; if (cols < 1) cols = 1;
    float cw = 220.f / cols; if (cw > 60.f) cw = 60.f;
    int rows = n < 5 ? n : 5;
    float ch = 190.f / rows; if (ch > 56.f) ch = 56.f;
    return { 8.f + (i / 5) * cw, 22.f + (i % 5) * ch, cw - 6.f, ch - 12.f };
}

// ---- pack loading -----------------------------------------------------------
struct LoadedPack {
    uint8_t* data = nullptr;
    size_t size = 0;
    Pack pack;
    const char* status = "";
    bool ok = false;
};

void load_pack(LoadedPack& lp) {
    FILE* f = fopen(kPackPath, "rb");
    if (!f) { lp.status = "No encounter pack on SD (placeholder fight only)"; return; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 16 * 1024 * 1024) { fclose(f); lp.status = "Encounter pack has an unexpected size"; return; }
    lp.data = static_cast<uint8_t*>(malloc(size_t(n)));
    if (!lp.data) { fclose(f); lp.status = "Not enough memory for the encounter pack"; return; }
    size_t got = fread(lp.data, 1, size_t(n), f);
    fclose(f);
    if (got != size_t(n) || !lp.pack.open(lp.data, got)) {
        free(lp.data); lp.data = nullptr;
        lp.status = "Encounter pack is corrupt or incomplete - rebuild and recopy it";
        return;
    }
    lp.size = got; lp.ok = true; lp.status = "Encounter pack loaded";
}

// ---- battle setup -----------------------------------------------------------
constexpr uint32_t kPartyAttack = 1, kEnemyAttack = 2;

void add_abilities(Battle& b) {
    Ability slash; slash.id = kPartyAttack; slash.hit_count = 4;
    for (int i = 0; i < 4; ++i) { slash.hit_frame[i] = u16(10 + i * 8); slash.hit_weight_pm[i] = 250; }
    Ability bite; bite.id = kEnemyAttack; bite.hit_count = 1; bite.hit_frame[0] = 20; bite.hit_weight_pm[0] = 1000;
    b.abilities = { slash, bite };
}

// Placeholder party, scaled to the mission's highest enemy level so any mission is fightable.
void add_party(Battle& b, int level) {
    for (int i = 0; i < kParty; ++i) {
        Unit u; u.side = Side::Party; u.level = level;
        u.max_hp = u.hp = 600 + 60 * level; u.max_mp = u.mp = 60;
        u.stat = { 60 + 8 * level + i * 3, 40 + 4 * level, 40 + 4 * level, 40 + 4 * level };
        u.abilities = { kPartyAttack };
        b.party.push_back(u);
    }
}

void placeholder_wave(std::vector<Wave>& waves) {
    Wave w; w.phase = 1;
    for (int i = 0; i < 3; ++i) {
        EnemySpec e; e.name = "Placeholder"; e.order = i + 1; e.level = 20; e.hp = 9000; e.atk = 140; e.def = 80; e.mag = 60; e.spr = 60;
        w.enemies.push_back(e);
    }
    waves.assign(1, w);
}

enum class Scene { Select, Battle, Result };
enum class Phase { Command, Resolving, Enemy };

} // namespace

int main() {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_text = C2D_TextBufNew(4096);

    bool is_new = false;
    APT_CheckNew3DS(&is_new);
    if (is_new) osSetSpeedupEnable(true);

    LoadedPack lp;
    load_pack(lp);
    std::vector<int32_t> missions;
    if (lp.ok) list_missions(lp.pack, missions);

    // Party sprites: slot i uses party/<i+1>.f3ds if it exists, else a placeholder rectangle.
    UnitSprite party_sprite[kParty];
    SpriteAnimator party_anim[kParty];
    int sprites_loaded = 0;
    for (int i = 0; i < kParty; ++i) {
        char path[64];
        snprintf(path, sizeof path, "%s/%d.f3ds", kPartyDir, i + 1);
        if (unit_sprite_load(path, party_sprite[i])) ++sprites_loaded;
    }
    // Presentation only: how a unit moves while acting. Never read by the battle rules.
    enum class Act : u8 { None, DashOut, Strike, DashBack };
    constexpr int kDashFrames = 10;
    Act act[kParty] = {};
    int act_t[kParty] = {};
    float dash_x[kParty] = {}, dash_y[kParty] = {};   // offset from the unit's home spot to its strike spot
    int win_stage[kParty] = {};                        // 0 not started, 1 playing win_before, 2 looping win
    auto home_x = [](int i) { return 268.f + (i % 2) * 60.f + 36.f; };
    auto home_y = [](int i) { return 30.f + (i / 2) * 60.f + (i % 2) * 14.f + 44.f; };
    auto play_once = [&](int i, const char* name) {
        int a = party_sprite[i].ok ? party_sprite[i].sprite.find_anim(name) : -1;
        if (a < 0) return false;
        party_anim[i].play(party_sprite[i].sprite, a, false);
        return true;
    };
    auto reset_acts = [&]() { for (int i = 0; i < kParty; ++i) { act[i] = Act::None; act_t[i] = 0; win_stage[i] = 0; } };
    auto set_anim = [&](int i, const char* name, bool loop) {
        if (!party_sprite[i].ok) return;
        int a = party_sprite[i].sprite.find_anim(name);
        if (a < 0) a = party_sprite[i].sprite.find_anim("idle");
        if (a < 0) a = 0;
        if (party_anim[i].anim != a || party_anim[i].finished) party_anim[i].play(party_sprite[i].sprite, a, loop);
    };

    Scene scene = Scene::Select;
    bool show_diag = false;
    u64 last_tick = svcGetSystemTick();
    float frame_ms = 0, worst_ms = 0;
    u32 frame_no = 0;

    // Select state
    int sel = 0;
    bool preview_dirty = true;
    std::vector<Wave> waves;
    EncounterError preview_err = EncounterError::Ok;
    int32_t preview_bad = 0;

    // Battle state
    Battle battle;
    std::vector<Action> queued;
    std::vector<HitEvent> events;
    bool fired[kParty] = {};
    int focus = 0, target = 0, wave_idx = 0, wait = 0, last_chain = 0;
    long long last_damage = 0;
    u32 turn_frame = 0;
    bool clock_running = false, confirm_retreat = false;
    std::vector<SkillInfo> skill_names;          // names of loaded enemy skills (point into the pack)
    const char* enemy_log[3] = { "", "", "" };  // last distinct enemy skills used
    int enemy_log_n = 0;
    Phase phase = Phase::Command;
    Outcome result = Outcome::Ongoing;

    auto reset_turn = [&]() {
        queued.clear();
        for (bool& f : fired) f = false;
        turn_frame = 0; clock_running = false;
    };
    auto start_wave = [&](int idx) {
        wave_idx = idx;
        spawn_wave(waves[idx], battle.enemies);
        for (size_t i = 0; i < battle.enemies.size(); ++i) {
            Unit& e = battle.enemies[i];
            if (lp.ok) load_monster_skills(lp.pack, waves[idx].enemies[i].skill_set_id, battle.abilities, e.abilities, skill_names);
            if (e.abilities.empty()) e.abilities = { kEnemyAttack };   // no damage skill decoded: generic attack
        }
        target = 0; phase = Phase::Command;
        reset_turn();
        reset_acts();
    };
    auto next_target = [&]() {
        int n = int(battle.enemies.size());
        for (int k = 1; k <= n; ++k) { int t = (target + k) % n; if (battle.enemies[t].alive()) { target = t; return; } }
    };
    auto activate = [&](int i) {
        if (phase != Phase::Command || fired[i] || !battle.party[i].alive()) return;
        if (!battle.enemies[target].alive()) next_target();
        if (!battle.enemies[target].alive()) return;
        fired[i] = true;
        {   // Run up to the target, strike, run back.
            Rect r = enemy_rect(target, int(battle.enemies.size()));
            dash_x[i] = (r.x + r.w + 30.f) - home_x(i);
            dash_y[i] = (r.y + r.h) - home_y(i);
            act[i] = Act::DashOut; act_t[i] = 0;
        }
        clock_running = true;            // chain timing is measured from the first press
        queued.push_back({ u8(i), kPartyAttack, u8(target), u16(turn_frame) });
    };

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        touchPosition tp{};
        bool touched = down & KEY_TOUCH;
        if (touched) hidTouchRead(&tp);
        if (down & KEY_SELECT) { show_diag = !show_diag; worst_ms = 0; }

        // =================== update ===================
        if (scene == Scene::Select) {
            if (down & KEY_START) break;
            int n = int(missions.size());
            if (n > 0) {
                int step = 0;
                if (down & KEY_DRIGHT) step = 1;
                if (down & KEY_DLEFT) step = -1;
                if (down & KEY_DDOWN) step = 10;
                if (down & KEY_DUP) step = -10;
                if (down & KEY_R) step = 100;
                if (down & KEY_L) step = -100;
                if (step) { sel = ((sel + step) % n + n) % n; preview_dirty = true; }
                if (preview_dirty) {
                    Rng rng(u32(svcGetSystemTick()) | 1u);
                    preview_err = load_encounter(lp.pack, missions[sel], rng, waves, &preview_bad);
                    preview_dirty = false;
                }
            } else if (waves.empty()) {
                placeholder_wave(waves);
            }
            if ((down & KEY_A) && !waves.empty()) {
                int level = 1;
                for (const Wave& w : waves) for (const EnemySpec& e : w.enemies) if (e.level > level) level = e.level;
                battle = Battle(u32(svcGetSystemTick()) | 1u);
                add_abilities(battle);
                add_party(battle, level);
                focus = 0; last_chain = 0; last_damage = 0; confirm_retreat = false;
                skill_names.clear(); enemy_log_n = 0;
                start_wave(0);
                scene = Scene::Battle;
            }
        } else if (scene == Scene::Battle) {
            if (confirm_retreat) {
                // The dialog owns all input until it is answered.
                if (down & KEY_A) { result = Outcome::Defeat; scene = Scene::Result; confirm_retreat = false; }
                else if (down & KEY_B) confirm_retreat = false;
            } else if (phase == Phase::Command) {
                if (down & KEY_START) confirm_retreat = true;
                if (down & KEY_DRIGHT) focus = (focus % 2 == 0) ? focus + 1 : focus - 1;
                if (down & KEY_DLEFT)  focus = (focus % 2 == 1) ? focus - 1 : focus + 1;
                if (down & KEY_DDOWN)  focus = (focus + 2) % kParty;
                if (down & KEY_DUP)    focus = (focus + 4) % kParty;
                if (down & KEY_R) focus = (focus + 1) % kParty;
                if (down & KEY_L) focus = (focus + kParty - 1) % kParty;
                if (down & KEY_Y) next_target();
                if (down & KEY_A) activate(focus);
                if (down & KEY_X) for (int i = 0; i < kParty; ++i) activate(i);
                if (touched) for (int i = 0; i < kParty; ++i) if (inside(card(i), tp)) { focus = i; activate(i); }
                bool all = true;
                for (int i = 0; i < kParty; ++i) if (battle.party[i].alive() && !fired[i]) all = false;
                if (!queued.empty() && (all || (down & KEY_B))) phase = Phase::Resolving;
                if (clock_running) ++turn_frame;   // fixed 60 Hz simulation clock
            } else if (phase == Phase::Resolving) {
                events.clear();
                battle.resolve(Side::Party, queued, events);
                last_damage = 0; last_chain = 0;
                for (const HitEvent& e : events) { last_damage += e.damage; if (e.chain_count > last_chain) last_chain = e.chain_count; }
                phase = Phase::Enemy; wait = 45;
            } else if (--wait <= 0) {
                Outcome o = battle.outcome();
                if (o == Outcome::Victory && wave_idx + 1 < int(waves.size())) {
                    battle.end_round();
                    start_wave(wave_idx + 1);
                } else if (o != Outcome::Ongoing) {
                    result = o; scene = Scene::Result;
                } else {
                    events.clear();
                    battle.enemy_turn(events);
                    enemy_log_n = 0;
                    for (const HitEvent& e : events) {
                        const char* nm = e.ability == kEnemyAttack ? "Attack" : nullptr;
                        for (const SkillInfo& si : skill_names) if (si.id == e.ability) { nm = si.name; break; }
                        bool dup = false;
                        for (int k = 0; k < enemy_log_n; ++k) if (enemy_log[k] == nm) dup = true;
                        if (nm && !dup && enemy_log_n < 3) enemy_log[enemy_log_n++] = nm;
                    }
                    battle.end_round();
                    reset_turn();
                    if (!battle.enemies[target].alive()) next_target();
                    if (battle.outcome() == Outcome::Defeat) { result = Outcome::Defeat; scene = Scene::Result; }
                    else phase = Phase::Command;
                }
            }
        } else {   // Result
            if (down & (KEY_A | KEY_B)) { scene = Scene::Select; preview_dirty = true; if (missions.empty()) waves.clear(); }
        }

        // Sprite animation runs on the same 60 Hz clock as the simulation.
        if (scene != Scene::Select)
            for (int i = 0; i < kParty; ++i) {
                if (!party_sprite[i].ok) continue;
                const Unit& u = battle.party[i];
                if (!u.alive()) {
                    act[i] = Act::None; win_stage[i] = 0;
                    set_anim(i, "dead", true);
                } else if (scene == Scene::Result && result == Outcome::Victory) {
                    act[i] = Act::None;
                    // Lead-in once, then the looping pose.
                    if (win_stage[i] == 0) win_stage[i] = play_once(i, "win_before") ? 1 : 2;
                    if (win_stage[i] == 1 && party_anim[i].finished) win_stage[i] = 2;
                    if (win_stage[i] == 2) set_anim(i, "win", true);
                } else {
                    win_stage[i] = 0;
                    switch (act[i]) {
                        case Act::DashOut:
                            set_anim(i, "move", true);
                            if (++act_t[i] >= kDashFrames) {
                                if (play_once(i, "atk")) act[i] = Act::Strike;
                                else { act[i] = Act::DashBack; act_t[i] = 0; }
                            }
                            break;
                        case Act::Strike:
                            if (party_anim[i].finished) { act[i] = Act::DashBack; act_t[i] = 0; }
                            break;
                        case Act::DashBack:
                            set_anim(i, "move", true);
                            if (++act_t[i] >= kDashFrames) act[i] = Act::None;
                            break;
                        case Act::None:
                            set_anim(i, u.hp * 4 < u.max_hp ? "dying" : "idle", true);
                            break;
                    }
                }
                party_anim[i].advance(party_sprite[i].sprite);
            }

        // =================== render ===================
        u64 now = svcGetSystemTick();
        frame_ms = float(double(now - last_tick) * 1000.0 / double(SYSCLOCK_ARM11));
        last_tick = now;
        if (++frame_no > 30 && frame_ms > worst_ms) worst_ms = frame_ms;

        C2D_TextBufClear(g_text);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // ---- top screen ----
        C2D_TargetClear(top, kBg);
        C2D_SceneBegin(top);
        if (scene == Scene::Select) {
            text(12, 14, 0.8f, kWhite, "FFBE 3D");
            text(12, 40, 0.5f, kDim, "development build - placeholder graphics");
            text(12, 70, 0.5f, lp.ok ? kHp : kFocus, "%s", lp.status);
            if (lp.ok) text(12, 88, 0.5f, kWhite, "%d missions with battles", int(missions.size()));
            else text(12, 88, 0.45f, kWhite, "Expected file: %s", kPackPath);
            text(12, 104, 0.45f, sprites_loaded ? kHp : kDim, "Party sprites loaded: %d of %d", sprites_loaded, kParty);
            if (!waves.empty()) {
                const Wave& w = waves[0];
                text(12, 120, 0.5f, kWhite, "First wave: %d enem%s", int(w.enemies.size()), w.enemies.size() == 1 ? "y" : "ies");
                for (size_t i = 0; i < w.enemies.size() && i < 5; ++i)
                    text(20, 138 + i * 16, 0.45f, kWhite, "Lv %d  %s  HP %lld", w.enemies[i].level, w.enemies[i].name, (long long)w.enemies[i].hp);
            }
            text(12, 222, 0.42f, kDim, is_new ? "New 3DS" : "Old 3DS (unsupported, may be slow)");
        } else {
            int n = int(battle.enemies.size());
            for (int i = 0; i < n; ++i) {
                const Unit& e = battle.enemies[i];
                Rect r = enemy_rect(i, n);
                C2D_DrawRectSolid(r.x, r.y, 0.3f, r.w, r.h, e.alive() ? kFoe : kDim);
                bar(r.x, r.y - 6, r.w, 4, e.hp, e.max_hp);
                if (i == target && scene == Scene::Battle) C2D_DrawRectSolid(r.x - 2, r.y + r.h + 1, 0.35f, r.w + 4, 3, kFocus);
            }
            for (int i = 0; i < kParty; ++i) {
                const Unit& u = battle.party[i];
                float x = 268.f + (i % 2) * 60.f, y = 30.f + (i / 2) * 60.f + (i % 2) * 14.f;
                // How far along the run-up this unit is: 0 at home, 1 at the strike spot (eased).
                float t = act[i] == Act::Strike ? 1.f
                        : act[i] == Act::DashOut ? float(act_t[i]) / kDashFrames
                        : act[i] == Act::DashBack ? 1.f - float(act_t[i]) / kDashFrames : 0.f;
                t = t * t * (3.f - 2.f * t);
                SpriteStep st;
                if (party_sprite[i].ok && party_anim[i].current(party_sprite[i].sprite, st))
                    unit_sprite_draw(party_sprite[i], st.frame, home_x(i) + dash_x[i] * t + st.dx * kUnitSize,
                                     home_y(i) + dash_y[i] * t + st.dy * kUnitSize, kUnitSize, 0.3f);
                else
                    C2D_DrawRectSolid(x, y, 0.3f, 36, 44, !u.alive() ? kDim : fired[i] ? kPanel : kAlly);
            }
            text(6, 2, 0.45f, kWhite, "Wave %d/%d   Turn %d", wave_idx + 1, int(waves.size()), battle.turn);
            if (last_chain > 1) text(170, 2, 0.5f, kFocus, "%d CHAIN  %lld", last_chain, last_damage);
            if (n > 0 && target < n) {
                const EnemySpec& spec = waves[wave_idx].enemies[target];
                const Unit& e = battle.enemies[target];
                text(6, 222, 0.45f, kWhite, "Lv %d %s  %lld/%lld", spec.level, spec.name, (long long)e.hp, (long long)e.max_hp);
            }
            if (scene == Scene::Battle && phase == Phase::Command)
                for (int k = 0; k < enemy_log_n; ++k) text(240, 200 - (enemy_log_n - 1 - k) * 13, 0.4f, kFoe, "> %s", enemy_log[k]);
            if (scene == Scene::Result) {
                C2D_DrawRectSolid(0, 90, 0.5f, 400, 60, kShade);
                text(140, 108, 0.9f, kWhite, result == Outcome::Victory ? "VICTORY" : "DEFEAT");
            }
            if (confirm_retreat) {
                C2D_DrawRectSolid(60, 80, 0.5f, 280, 80, kShade);
                text(96, 96, 0.6f, kWhite, "Retreat from this battle?");
                text(96, 126, 0.5f, kFocus, "A: retreat     B: keep fighting");
            }
        }
        if (show_diag) {
            C2D_DrawRectSolid(250, 0, 0.55f, 150, 46, kShade);
            text(254, 2, 0.4f, kWhite, "frame %.1f ms  worst %.1f", frame_ms, worst_ms);
            text(254, 16, 0.4f, kWhite, "linear free %lu KB", (unsigned long)(linearSpaceFree() / 1024));
            text(254, 30, 0.4f, kWhite, "pack %lu KB", (unsigned long)(lp.size / 1024));
        }

        // ---- bottom screen ----
        C2D_TargetClear(bot, kBg);
        C2D_SceneBegin(bot);
        if (scene == Scene::Select) {
            if (!missions.empty()) {
                text(10, 10, 0.5f, kDim, "Mission %d of %d", sel + 1, int(missions.size()));
                text(10, 30, 0.9f, kWhite, "ID %ld", (long)missions[sel]);
                if (preview_err == EncounterError::Ok) text(10, 66, 0.5f, kHp, "%d wave%s", int(waves.size()), waves.size() == 1 ? "" : "s");
                else {
                    text(10, 66, 0.45f, kFocus, "Cannot load: %s", to_string(preview_err));
                    text(10, 82, 0.45f, kFocus, "(id %ld)", (long)preview_bad);
                }
                text(10, 150, 0.45f, kWhite, "Left/Right: +-1   Up/Down: +-10   L/R: +-100");
            } else {
                text(10, 30, 0.6f, kWhite, "Placeholder fight");
            }
            text(10, 170, 0.45f, kWhite, waves.empty() ? "" : "A: start battle");
            text(10, 204, 0.42f, kDim, "SELECT: diagnostics   START: exit");
        } else if (scene == Scene::Battle) {
            for (int i = 0; i < kParty; ++i) {
                const Unit& u = battle.party[i];
                Rect r = card(i);
                if (i == focus) C2D_DrawRectSolid(r.x - 2, r.y - 2, 0.2f, r.w + 4, r.h + 4, kFocus);
                C2D_DrawRectSolid(r.x, r.y, 0.3f, r.w, r.h, (!u.alive() || fired[i]) ? kDim : kPanel);
                text(r.x + 6, r.y + 3, 0.5f, kWhite, "Unit %d%s", i + 1, !u.alive() ? "  KO" : fired[i] ? "  acted" : "");
                text(r.x + 6, r.y + 20, 0.45f, kWhite, "HP %lld/%lld", (long long)u.hp, (long long)u.max_hp);
                bar(r.x + 6, r.y + 38, r.w - 12, 5, u.hp, u.max_hp);
                text(r.x + 6, r.y + 44, 0.38f, kWhite, "MP %d", u.mp);
            }
            text(4, 192, 0.42f, kWhite, "A act  X all  Y target  L/R unit  B end turn");
            text(4, 208, 0.42f, kDim, "START retreat  SELECT diagnostics  touch a card");
        } else {
            text(10, 30, 0.7f, kWhite, result == Outcome::Victory ? "Mission clear" : "Mission failed");
            text(10, 70, 0.5f, kWhite, "Turns: %d", battle.turn);
            text(10, 90, 0.45f, kDim, "Rewards and saving are not implemented yet.");
            text(10, 170, 0.45f, kWhite, "A: back to mission select");
        }

        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(g_text);
    for (UnitSprite& u : party_sprite) unit_sprite_free(u);
    free(lp.data);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
