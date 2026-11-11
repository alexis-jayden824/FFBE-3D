// Host test for the sprite file reader and animator, against a synthetic unit
// (invented coloured blocks) produced by tests/content/make_sprite_fixture.py.
#include "content/sprite.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace f3d;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: test_sprite <file.f3ds>\n"); return 1; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    Sprite s;
    CHECK(s.open(blob.data(), blob.size()));
    CHECK(s.atlas_w() == 64 && s.atlas_h() == 64 && s.texture_size() == 64 * 64 * 4);
    CHECK(s.rect_count() == 3 && s.frame_count() == 5 && s.anim_count() == 3);
    CHECK(s.scale_permille() == 500);
    {   // Tables-only open: the blob may end where the texture starts, but no earlier.
        uint32_t tex_off = uint32_t(blob.size()) - s.texture_size();
        Sprite t;
        CHECK(t.open(blob.data(), tex_off, true) && t.frame_count() == 5 && t.find_anim("atk") == 1);
        CHECK(!t.open(blob.data(), tex_off, false));
        CHECK(!t.open(blob.data(), tex_off - 1, true));
    }
    CHECK(std::string(s.anim_name(0)) == "idle" && std::string(s.anim_name(1)) == "atk" && std::string(s.anim_name(2)) == "dead");
    CHECK(s.find_anim("atk") == 1 && s.find_anim("win") == -1);

    // idle: frame 0 for 6 ticks, then frame 1 (offset 0,-2 scaled to 0,-1) for 6 ticks.
    CHECK(s.anim_steps(0) == 2);
    SpriteStep st = s.step(0, 1);
    CHECK(st.delay == 6 && st.dx == 0 && st.dy == -1);

    // Frame 0 = red body drawn first, then the flipped green block on top (positions halved).
    uint16_t f0 = s.step(0, 0).frame;
    CHECK(s.frame_parts(f0) == 2);
    SpritePart body = s.part(f0, 0), head = s.part(f0, 1);
    SpriteRect rb = s.rect(body.rect), rh = s.rect(head.rect);
    CHECK(rb.w == 8 && rb.h == 12 && body.dx == -4 && body.dy == -12 && body.flip == 0 && body.opacity == 255 && body.blend == 0);
    CHECK(rh.w == 4 && rh.h == 4 && head.dx == -2 && head.dy == -20 && head.flip == 1);

    // atk step 1 uses the additive, half-opacity, rotated part.
    uint16_t f2 = s.step(1, 1).frame;
    SpritePart fx = s.part(f2, 1);
    CHECK(s.frame_parts(f2) == 2 && fx.blend == 1 && fx.opacity == 127 && fx.rot == 90 && s.rect(fx.rect).w == 6);
    CHECK(s.step(1, 1).dx == -3);
    // The frame whose only part was out of the sheet is kept but empty; so is the empty frame.
    CHECK(s.frame_parts(s.step(1, 2).frame) == 0 && s.frame_parts(s.step(2, 0).frame) == 0);

    // Animator: looping idle.
    SpriteAnimator a;
    a.play(s, 0, true);
    SpriteStep cur;
    for (int i = 0; i < 5; ++i) a.advance(s);
    CHECK(a.current(s, cur) && a.step == 0);
    a.advance(s);                                   // 6th tick -> step 1
    CHECK(a.step == 1 && !a.finished);
    for (int i = 0; i < 6; ++i) a.advance(s);       // wraps
    CHECK(a.step == 0 && !a.finished);
    // One-shot atk: 4 + 8 + 2 ticks, then finished and holding the last step.
    a.play(s, 1, false);
    for (int i = 0; i < 13; ++i) a.advance(s);
    CHECK(!a.finished && a.step == 2);
    a.advance(s);
    CHECK(a.finished && a.step == 2);
    for (int i = 0; i < 50; ++i) a.advance(s);
    CHECK(a.finished && a.step == 2 && a.current(s, cur));
    // Bad animation index is safe.
    a.play(s, 99, true);
    CHECK(a.finished && !a.current(s, cur));
    a.advance(s);

    // Corrupt files are rejected.
    Sprite bad;
    CHECK(!bad.open(nullptr, 0) && !bad.open(blob.data(), 20));
    std::vector<uint8_t> t(blob.begin(), blob.end() - 1);
    CHECK(!bad.open(t.data(), t.size()));                       // truncated texture
    t = blob; t[0] = 'X'; CHECK(!bad.open(t.data(), t.size())); // magic
    t = blob; t[8] = 63; CHECK(!bad.open(t.data(), t.size()));  // atlas not a power of two
    t = blob; t[36 + 4] = 0xff; t[36 + 5] = 0xff; CHECK(!bad.open(t.data(), t.size()));   // rect wider than atlas
    uint32_t parts_off = 36 + 3 * 8 + 5 * 8;
    t = blob; t[parts_off] = 200; CHECK(!bad.open(t.data(), t.size()));                   // part -> missing rect
    uint32_t steps_off = parts_off + 5 * 12 + 3 * 24;   // 5 parts survive (2 + 1 + 2), 3 animations
    t = blob; t[steps_off] = 77; CHECK(!bad.open(t.data(), t.size()));                    // step -> missing frame
    CHECK(!bad.valid());

    std::printf(g_fail ? "%d check(s) failed\n" : "all sprite tests passed\n", g_fail);
    return g_fail ? 1 : 0;
}
