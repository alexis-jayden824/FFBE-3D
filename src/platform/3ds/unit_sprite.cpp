#include "unit_sprite.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr uint32_t kHeader = 36;
constexpr uint32_t kMaxTables = 4u << 20;
uint32_t rd32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

void blend_normal()   { C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA); }
void blend_additive() { C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE); }
}

bool unit_sprite_load(const char* path, UnitSprite& u) {
    u.ok = false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    uint8_t head[kHeader];
    if (fread(head, 1, kHeader, f) != kHeader) { fclose(f); return false; }
    uint32_t tex_off = rd32(head + 28), tex_size = rd32(head + 32);
    if (tex_off < kHeader || tex_off > kMaxTables) { fclose(f); return false; }

    uint8_t* tables = static_cast<uint8_t*>(malloc(tex_off));
    if (!tables) { fclose(f); return false; }
    memcpy(tables, head, kHeader);
    if (fread(tables + kHeader, 1, tex_off - kHeader, f) != tex_off - kHeader || !u.sprite.open(tables, tex_off, true)) {
        free(tables); fclose(f); return false;
    }
    // open() has checked the atlas is a power of two up to 1024 and tex_size == w * h * 4.
    if (!C3D_TexInit(&u.tex, u.sprite.atlas_w(), u.sprite.atlas_h(), GPU_RGBA8)) {
        free(tables); fclose(f); u.sprite = f3d::Sprite(); return false;      // out of video memory
    }
    if (fread(u.tex.data, 1, tex_size, f) != tex_size) {
        C3D_TexDelete(&u.tex); free(tables); fclose(f); u.sprite = f3d::Sprite(); return false;
    }
    fclose(f);
    // Art converted at original size is drawn 1:1, so keep pixels crisp; shrunk art is magnified, so smooth it.
    GPU_TEXTURE_FILTER_PARAM filter = u.sprite.scale_permille() == 1000 ? GPU_NEAREST : GPU_LINEAR;
    C3D_TexSetFilter(&u.tex, filter, filter);
    C3D_TexFlush(&u.tex);
    u.tables = tables;
    u.ok = true;
    return true;
}

void unit_sprite_free(UnitSprite& u) {
    if (!u.ok) return;
    C3D_TexDelete(&u.tex);
    free(u.tables);
    u.tables = nullptr;
    u.sprite = f3d::Sprite();
    u.ok = false;
}

void unit_sprite_draw(const UnitSprite& u, uint16_t frame, float ox, float oy, float size, float depth) {
    if (!u.ok || frame >= u.sprite.frame_count()) return;
    const float k = size * 1000.0f / float(u.sprite.scale_permille());
    const float W = float(u.sprite.atlas_w()), H = float(u.sprite.atlas_h());
    bool additive = false;
    const uint16_t n = u.sprite.frame_parts(frame);
    for (uint16_t i = 0; i < n; ++i) {
        const f3d::SpritePart p = u.sprite.part(frame, i);
        if (p.opacity == 0) continue;
        const f3d::SpriteRect r = u.sprite.rect(p.rect);

        const bool add = p.blend == 1;
        if (add != additive) {              // blend mode is GPU state: flush what is queued, then switch
            C2D_Flush();
            if (add) blend_additive(); else blend_normal();
            additive = add;
        }

        // A vertical mirror is a horizontal mirror plus a half turn. Doing it that
        // way keeps top > bottom in the sub-texture, which citro2d requires
        // (top < bottom means "stored rotated" to it).
        bool mirror = p.flip & 1;
        int rot = ((p.rot % 360) + 360) % 360;
        if (p.flip & 2) { mirror = !mirror; rot = (rot + 180) % 360; }

        float left = r.x / W, right = (r.x + r.w) / W;
        if (mirror) { float t = left; left = right; right = t; }
        const Tex3DS_SubTexture sub = { r.w, r.h, left, 1.0f - r.y / H, right, 1.0f - (r.y + r.h) / H };
        const C2D_Image img = { const_cast<C3D_Tex*>(&u.tex), &sub };

        // dx/dy locate the top-left of the part's bounding box after its own rotation.
        const bool quarter = (p.rot % 180) != 0;
        const float w = r.w * k, h = r.h * k;
        const float bw = quarter ? h : w, bh = quarter ? w : h;
        const C2D_DrawParams prm = {
            { ox + p.dx * k + bw * 0.5f, oy + p.dy * k + bh * 0.5f, w, h },
            { w * 0.5f, h * 0.5f },
            depth,
            float(rot) * 3.14159265f / 180.0f,
        };
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, p.opacity / 255.0f);
        C2D_DrawImage(img, &prm, &tint);
    }
    if (additive) { C2D_Flush(); blend_normal(); }
}
