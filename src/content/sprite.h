// Console sprite file (.f3ds) written by tools/assets/unitsprite.py.
//
// A sprite is an atlas texture plus tables: rectangles in the atlas, frames
// (each a list of parts in draw order), and named animations (each a list of
// steps: frame, offset, delay in 1/60 s). Zero-copy: open() validates every
// index once, accessors then read straight from the blob.
//
// Layout (little endian), header 36 bytes:
//   "F3DS" u32 version(2) u16 atlas_w u16 atlas_h u16 rects u16 frames
//   u32 parts u16 anims u16 scale_permille u32 steps u32 tex_offset u32 tex_size
//   rects:  u16 x, y, w, h                                   (8 bytes)
//   frames: u32 first_part, u16 part_count, u16 pad          (8 bytes)
//   parts:  u16 rect, i16 dx, dy, rot_degrees, u8 flip, blend, opacity, pad   (12 bytes)
//   anims:  char name[16], u32 first_step, u16 step_count, u16 pad            (24 bytes)
//   steps:  u16 frame, i16 dx, dy, u16 delay                 (8 bytes)
//   texture: atlas_w * atlas_h * 4 bytes, console-native (top row first,
//            8x8 tiles, Z-order, bytes A,B,G,R). Version 1 stored it bottom row
//            first, which drew garbled on hardware; version 1 files are rejected.
//
// All positions and sizes are in atlas texels, i.e. source pixels multiplied by
// scale_permille / 1000. To draw at a given size relative to the original art,
// multiply everything by (wanted_scale * 1000 / scale_permille).
//
// Part dx/dy is the top-left of the part's bounding box after rotation,
// relative to the unit's origin. flip: bit 0 = horizontal, bit 1 = vertical.
// blend: 0 = normal, 1 = additive. opacity: 0-255.
#pragma once
#include <cstddef>
#include <cstdint>

namespace f3d {

struct SpriteRect { uint16_t x, y, w, h; };
struct SpritePart { uint16_t rect; int16_t dx, dy, rot; uint8_t flip, blend, opacity; };
struct SpriteStep { uint16_t frame; int16_t dx, dy; uint16_t delay; };

class Sprite {
public:
    // False if malformed; does not copy. With tables_only, `data` may stop at the
    // texture offset (the console streams the texture straight into video memory
    // instead of holding a second copy); texture() must then not be used.
    bool open(const uint8_t* data, size_t size, bool tables_only = false);
    bool valid() const { return d_ != nullptr; }

    uint16_t atlas_w() const { return w_; }
    uint16_t atlas_h() const { return h_; }
    uint16_t rect_count() const { return nrect_; }
    uint16_t frame_count() const { return nframe_; }
    uint16_t anim_count() const { return nanim_; }
    uint16_t scale_permille() const { return scale_; }

    SpriteRect rect(uint16_t i) const;
    uint16_t frame_parts(uint16_t frame) const;                 // number of parts in a frame
    SpritePart part(uint16_t frame, uint16_t i) const;          // i-th part, in draw order
    const char* anim_name(uint16_t anim) const;
    int find_anim(const char* name) const;                      // -1 if absent
    uint16_t anim_steps(uint16_t anim) const;
    SpriteStep step(uint16_t anim, uint16_t i) const;

    const uint8_t* texture() const { return d_ + tex_off_; }
    uint32_t texture_size() const { return tex_size_; }

private:
    const uint8_t* d_ = nullptr;
    uint16_t w_ = 0, h_ = 0, nrect_ = 0, nframe_ = 0, nanim_ = 0, scale_ = 1000;
    uint32_t npart_ = 0, nstep_ = 0, tex_off_ = 0, tex_size_ = 0;
    uint32_t rects_ = 0, frames_ = 0, parts_ = 0, anims_ = 0, steps_ = 0;
};

// Plays one animation on the 60 Hz simulation clock.
struct SpriteAnimator {
    int anim = -1;
    uint16_t step = 0, tick = 0;
    bool loop = true, finished = false;

    void play(const Sprite& s, int anim_index, bool looping);
    void advance(const Sprite& s);                  // call once per simulation frame
    bool current(const Sprite& s, SpriteStep& out) const;
};

} // namespace f3d
