// Milestone 1 platform shell: both screens, buttons, Circle Pad, touch, and the
// portable battle core running on a fixed 60 Hz simulation clock.
//
// Everything drawn here is placeholder geometry and placeholder units. It
// exists to prove the platform layer and the input model, not to look like
// the game. STATUS: not yet compiled or run on hardware/emulator - see
// docs/TEST_REPORT.md.
#include <3ds.h>
#include <citro2d.h>
#include <cstdarg>
#include <cstdio>
#include <vector>

#include "core/battle.h"

using namespace f3d;

namespace {

constexpr int kTopW = 400, kBotW = 320, kH = 240;
constexpr u32 kBg = C2D_Color32(18, 20, 28, 255);
constexpr u32 kPanel = C2D_Color32(40, 46, 64, 255);
constexpr u32 kFocus = C2D_Color32(240, 200, 80, 255);
constexpr u32 kHp = C2D_Color32(80, 200, 120, 255);
constexpr u32 kHpBack = C2D_Color32(70, 30, 30, 255);
constexpr u32 kAlly = C2D_Color32(90, 140, 230, 255);
constexpr u32 kFoe = C2D_Color32(210, 90, 90, 255);
constexpr u32 kDim = C2D_Color32(90, 90, 100, 255);
constexpr u32 kWhite = C2D_Color32(255, 255, 255, 255);

C2D_TextBuf g_text;

void text(float x, float y, float size, u32 color, const char* fmt, ...) {
    char buf[96];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    C2D_Text t;
    C2D_TextParse(&t, g_text, buf);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, size, size, color);
}

void bar(float x, float y, float w, float h, int cur, int max) {
    C2D_DrawRectSolid(x, y, 0.4f, w, h, kHpBack);
    if (max > 0 && cur > 0) C2D_DrawRectSolid(x, y, 0.45f, w * cur / max, h, kHp);
}

// Placeholder encounter so the shell has something to simulate.
Battle make_demo() {
    Battle b(svcGetSystemTick() & 0xffffffffu);
    Ability slash; slash.id = 1; slash.hit_count = 4;
    for (int i = 0; i < 4; ++i) { slash.hit_frame[i] = u16(10 + i * 8); slash.hit_weight_pm[i] = 250; }
    Ability bite; bite.id = 2; bite.hit_count = 1; bite.hit_frame[0] = 20; bite.hit_weight_pm[0] = 1000;
    b.abilities = { slash, bite };
    for (int i = 0; i < 6; ++i) {
        Unit u; u.side = Side::Party; u.level = 20; u.max_hp = u.hp = 900; u.max_mp = u.mp = 60;
        u.stat = { 120 + i * 5, 70, 60, 60 }; u.abilities = { 1 };
        b.party.push_back(u);
    }
    for (int i = 0; i < 3; ++i) {
        Unit e; e.side = Side::Enemy; e.level = 20; e.max_hp = e.hp = 9000;
        e.stat = { 140, 80, 60, 60 }; e.abilities = { 2 };
        b.enemies.push_back(e);
    }
    return b;
}

// Bottom screen: 2 columns x 3 rows of command cards, like the original layout
// but sized for a stylus.
struct Rect { float x, y, w, h; };
Rect card(int i) { return { 4.f + (i % 2) * 158.f, 4.f + (i / 2) * 66.f, 154.f, 62.f }; }

enum class Phase { Command, Resolving, Enemy, Done };

} // namespace

int main() {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_text = C2D_TextBufNew(2048);

    bool is_new = false;
    APT_CheckNew3DS(&is_new);
    if (is_new) osSetSpeedupEnable(true);

    Battle battle = make_demo();
    std::vector<Action> queued;          // this turn's activations, in press order
    std::vector<HitEvent> events;
    bool fired[kMaxPerSide] = {};
    int focus = 0, target = 0;
    u32 turn_frame = 0;                  // simulation frames since the first activation
    bool clock_running = false;
    Phase phase = Phase::Command;
    int last_chain = 0, last_damage = 0, wait = 0;

    auto activate = [&](int i) {
        if (phase != Phase::Command || fired[i] || !battle.party[i].alive()) return;
        while (target < int(battle.enemies.size()) && !battle.enemies[target].alive()) ++target;
        if (target >= int(battle.enemies.size())) return;
        fired[i] = true;
        clock_running = true;            // chain timing is measured from the first press
        queued.push_back({ u8(i), 1, u8(target), u16(turn_frame) });
    };

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START) break;

        // ---- input (Command phase) ------------------------------------------
        if (phase == Phase::Command) {
            if (down & KEY_DRIGHT) focus = (focus % 2 == 0) ? focus + 1 : focus - 1;
            if (down & KEY_DLEFT)  focus = (focus % 2 == 1) ? focus - 1 : focus + 1;
            if (down & KEY_DDOWN)  focus = (focus + 2) % 6;
            if (down & KEY_DUP)    focus = (focus + 4) % 6;
            if (down & KEY_R) focus = (focus + 1) % 6;
            if (down & KEY_L) focus = (focus + 5) % 6;
            if (down & KEY_Y) target = (target + 1) % int(battle.enemies.size());
            if (down & KEY_A) activate(focus);
            if (down & KEY_X) for (int i = 0; i < 6; ++i) activate(i);   // "fire all" convenience
            if (down & KEY_TOUCH) {
                touchPosition tp; hidTouchRead(&tp);
                for (int i = 0; i < 6; ++i) {
                    Rect r = card(i);
                    if (tp.px >= r.x && tp.px < r.x + r.w && tp.py >= r.y && tp.py < r.y + r.h) { focus = i; activate(i); }
                }
            }
            // Turn ends when every living unit has acted, or on B once at least one has.
            bool all = true;
            for (int i = 0; i < 6; ++i) if (battle.party[i].alive() && !fired[i]) all = false;
            if (!queued.empty() && (all || (down & KEY_B))) phase = Phase::Resolving;
        }

        // ---- fixed-step simulation (one step per vblank) --------------------
        if (clock_running && phase == Phase::Command) ++turn_frame;
        if (phase == Phase::Resolving) {
            events.clear();
            battle.resolve(Side::Party, queued, events);
            last_damage = 0; last_chain = 0;
            for (const HitEvent& e : events) { last_damage += e.damage; if (e.chain_count > last_chain) last_chain = e.chain_count; }
            phase = battle.outcome() == Outcome::Ongoing ? Phase::Enemy : Phase::Done;
            wait = 45;
        } else if (phase == Phase::Enemy && --wait <= 0) {
            events.clear();
            battle.enemy_turn(events);
            battle.end_round();
            queued.clear();
            for (bool& f : fired) f = false;
            turn_frame = 0; clock_running = false;
            phase = battle.outcome() == Outcome::Ongoing ? Phase::Command : Phase::Done;
        } else if (phase == Phase::Done && (down & KEY_A)) {
            battle = make_demo();
            queued.clear();
            for (bool& f : fired) f = false;
            turn_frame = 0; clock_running = false; target = 0; phase = Phase::Command;
        }

        // ---- render ---------------------------------------------------------
        C2D_TextBufClear(g_text);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(top, kBg);
        C2D_SceneBegin(top);
        for (size_t i = 0; i < battle.enemies.size(); ++i) {
            const Unit& e = battle.enemies[i];
            float x = 30.f, y = 30.f + i * 62.f;
            C2D_DrawRectSolid(x, y, 0.3f, 56, 48, e.alive() ? kFoe : kDim);
            if (int(i) == target && phase == Phase::Command) C2D_DrawRectSolid(x - 3, y + 50, 0.35f, 62, 3, kFocus);
            bar(x, y - 7, 56, 4, e.hp, e.max_hp);
        }
        for (size_t i = 0; i < battle.party.size(); ++i) {
            const Unit& u = battle.party[i];
            float x = 250.f + (i % 2) * 60.f, y = 24.f + (i / 2) * 64.f + (i % 2) * 14.f;
            C2D_DrawRectSolid(x, y, 0.3f, 36, 44, !u.alive() ? kDim : fired[i] ? kPanel : kAlly);
        }
        text(8, 218, 0.5f, kWhite, "Turn %d", battle.turn);
        if (last_chain > 1) text(150, 6, 0.6f, kFocus, "%d CHAIN  %d", last_chain, last_damage);
        if (phase == Phase::Done)
            text(120, 105, 0.8f, kWhite, battle.outcome() == Outcome::Victory ? "VICTORY  (A: again)" : "DEFEAT  (A: again)");
        text(250, 222, 0.4f, kDim, is_new ? "New 3DS" : "Old 3DS (unsupported)");

        C2D_TargetClear(bot, kBg);
        C2D_SceneBegin(bot);
        for (int i = 0; i < 6; ++i) {
            const Unit& u = battle.party[i];
            Rect r = card(i);
            if (i == focus) C2D_DrawRectSolid(r.x - 2, r.y - 2, 0.2f, r.w + 4, r.h + 4, kFocus);
            C2D_DrawRectSolid(r.x, r.y, 0.3f, r.w, r.h, (!u.alive() || fired[i]) ? kDim : kPanel);
            text(r.x + 6, r.y + 4, 0.5f, kWhite, "Unit %d%s", i + 1, fired[i] ? "  *" : "");
            text(r.x + 6, r.y + 22, 0.45f, kWhite, "HP %d/%d", u.hp, u.max_hp);
            bar(r.x + 6, r.y + 40, r.w - 12, 5, u.hp, u.max_hp);
            text(r.x + 6, r.y + 47, 0.4f, kWhite, "MP %d", u.mp);
        }
        text(4, 204, 0.42f, kWhite, "A act  X all  Y target  L/R unit  B end");
        text(4, 220, 0.42f, kDim, "START exit   touch a card to act");

        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(g_text);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
