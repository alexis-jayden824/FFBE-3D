// Console side of unit sprites: loads a .f3ds file into a GPU texture and draws
// one animation frame with citro2d.
#pragma once
#include <3ds.h>
#include <citro2d.h>

#include "content/sprite.h"

struct UnitSprite {
    f3d::Sprite sprite;        // tables (rects, frames, animations)
    uint8_t* tables = nullptr; // backing memory for `sprite`
    C3D_Tex tex;
    bool ok = false;
};

// Returns false (and leaves `u.ok` false) if the file is missing, malformed, or
// there is not enough memory for its texture. Never leaves partial state behind.
bool unit_sprite_load(const char* path, UnitSprite& u);
void unit_sprite_free(UnitSprite& u);

// Draws `frame` with the unit's origin (its feet) at (ox, oy). `size` is the
// size relative to the original art (1.0 = one source pixel per screen pixel),
// whatever detail the texture was converted at.
void unit_sprite_draw(const UnitSprite& u, uint16_t frame, float ox, float oy, float size, float depth);
