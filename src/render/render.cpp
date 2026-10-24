// render/render.cpp
#include "render/render.hpp"
#include "raylib.h"
#include "rlgl.h"
#include "global/global.hpp"
#include "render/helpers_render.hpp"
#include "settings/settings.hpp"

void DrawFrame()
{
    const auto opacity = GetWindowOpacity(Global::Input::pointer.inside);
    // Preserve coverage in the key layer instead of squaring alpha on translucent glyphs/edges.
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA,
                              RL_FUNC_ADD, RL_FUNC_ADD);
    BeginTextureMode(Global::Render::texture_world);
    ClearBackground(BLANK);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    DrawKeyboard();
    EndBlendMode();
    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLANK);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    DrawRectangle(0, 0, S::screenWidth, S::screenHeight,
                  ColorAlpha(Default::BACKGROUND, opacity.background));
    EndBlendMode();
    const auto texture = Global::Render::texture_world.texture;
    // The key texture is premultiplied; fade RGB and alpha together before compositing it.
    const auto fade = static_cast<unsigned char>(255 * opacity.keys);
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTexturePro(texture,
                   {0, 0, static_cast<float>(texture.width), -static_cast<float>(texture.height)},
                   {0, 0, static_cast<float>(S::screenWidth), static_cast<float>(S::screenHeight)},
                   {0, 0}, 0, {fade, fade, fade, fade});
    EndBlendMode();
    EndDrawing();
}
