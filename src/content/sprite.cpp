#include "content/sprite.h"
#include <cstring>

namespace f3d {

namespace {
uint16_t r16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t r32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
bool pow2(uint32_t v) { return v && !(v & (v - 1)); }
constexpr uint32_t kHeader = 36;
}

bool Sprite::open(const uint8_t* data, size_t size, bool tables_only) {
    d_ = nullptr;
    if (!data || size < kHeader || std::memcmp(data, "F3DS", 4) != 0 || r32(data + 4) != 1) return false;
    uint16_t w = r16(data + 8), h = r16(data + 10), nrect = r16(data + 12), nframe = r16(data + 14);
    uint32_t npart = r32(data + 16);
    uint16_t nanim = r16(data + 20), scale = r16(data + 22);
    uint32_t nstep = r32(data + 24), tex_off = r32(data + 28), tex_size = r32(data + 32);

    if (!pow2(w) || !pow2(h) || w < 8 || h < 8 || w > 1024 || h > 1024) return false;
    uint64_t rects = kHeader, frames = rects + uint64_t(nrect) * 8, parts = frames + uint64_t(nframe) * 8;
    uint64_t anims = parts + uint64_t(npart) * 12, steps = anims + uint64_t(nanim) * 24, end = steps + uint64_t(nstep) * 8;
    if (end > size || tex_off < end) return false;
    if (!tables_only && uint64_t(tex_off) + tex_size > size) return false;
    if (tex_size != uint32_t(w) * h * 4) return false;
    if (!nframe || !nanim || !scale) return false;

    for (uint32_t i = 0; i < nrect; ++i) {
        const uint8_t* p = data + rects + i * 8;
        if (!r16(p + 4) || !r16(p + 6) || r16(p) + r16(p + 4) > w || r16(p + 2) + r16(p + 6) > h) return false;
    }
    for (uint32_t i = 0; i < nframe; ++i) {
        const uint8_t* p = data + frames + i * 8;
        if (uint64_t(r32(p)) + r16(p + 4) > npart) return false;
    }
    for (uint32_t i = 0; i < npart; ++i)
        if (r16(data + parts + i * 12) >= nrect) return false;
    for (uint32_t i = 0; i < nanim; ++i) {
        const uint8_t* p = data + anims + i * 24;
        if (p[15] != 0) return false;                                  // name must be terminated
        if (!r16(p + 20) || uint64_t(r32(p + 16)) + r16(p + 20) > nstep) return false;
    }
    for (uint32_t i = 0; i < nstep; ++i) {
        const uint8_t* p = data + steps + i * 8;
        if (r16(p) >= nframe || r16(p + 6) == 0) return false;
    }

    d_ = data; w_ = w; h_ = h; nrect_ = nrect; nframe_ = nframe; nanim_ = nanim;
    scale_ = scale; npart_ = npart; nstep_ = nstep; tex_off_ = tex_off; tex_size_ = tex_size;
    rects_ = uint32_t(rects); frames_ = uint32_t(frames); parts_ = uint32_t(parts);
    anims_ = uint32_t(anims); steps_ = uint32_t(steps);
    return true;
}

SpriteRect Sprite::rect(uint16_t i) const {
    const uint8_t* p = d_ + rects_ + uint32_t(i) * 8;
    return { r16(p), r16(p + 2), r16(p + 4), r16(p + 6) };
}

uint16_t Sprite::frame_parts(uint16_t frame) const { return r16(d_ + frames_ + uint32_t(frame) * 8 + 4); }

SpritePart Sprite::part(uint16_t frame, uint16_t i) const {
    uint32_t first = r32(d_ + frames_ + uint32_t(frame) * 8);
    const uint8_t* p = d_ + parts_ + (first + i) * 12;
    return { r16(p), int16_t(r16(p + 2)), int16_t(r16(p + 4)), int16_t(r16(p + 6)), p[8], p[9], p[10] };
}

const char* Sprite::anim_name(uint16_t anim) const {
    return reinterpret_cast<const char*>(d_ + anims_ + uint32_t(anim) * 24);
}

int Sprite::find_anim(const char* name) const {
    for (uint16_t i = 0; i < nanim_; ++i)
        if (std::strcmp(anim_name(i), name) == 0) return i;
    return -1;
}

uint16_t Sprite::anim_steps(uint16_t anim) const { return r16(d_ + anims_ + uint32_t(anim) * 24 + 20); }

SpriteStep Sprite::step(uint16_t anim, uint16_t i) const {
    uint32_t first = r32(d_ + anims_ + uint32_t(anim) * 24 + 16);
    const uint8_t* p = d_ + steps_ + (first + i) * 8;
    return { r16(p), int16_t(r16(p + 2)), int16_t(r16(p + 4)), r16(p + 6) };
}

void SpriteAnimator::play(const Sprite& s, int anim_index, bool looping) {
    if (!s.valid() || anim_index < 0 || anim_index >= s.anim_count()) { anim = -1; finished = true; return; }
    anim = anim_index; step = 0; tick = 0; loop = looping; finished = false;
}

void SpriteAnimator::advance(const Sprite& s) {
    if (anim < 0 || finished || !s.valid()) return;
    if (++tick < s.step(uint16_t(anim), step).delay) return;
    tick = 0;
    if (step + 1 < s.anim_steps(uint16_t(anim))) { ++step; return; }
    if (loop) step = 0; else finished = true;      // a finished one-shot holds its last step
}

bool SpriteAnimator::current(const Sprite& s, SpriteStep& out) const {
    if (anim < 0 || !s.valid()) return false;
    out = s.step(uint16_t(anim), step);
    return true;
}

} // namespace f3d
