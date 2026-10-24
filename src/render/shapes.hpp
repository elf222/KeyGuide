// render/shapes.hpp
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include "raylib.h"
#include "global/global.hpp"

inline Font HintFont()
{
    return Global::Render::font.texture.id ? Global::Render::font : GetFontDefault();
}

// Pen advance includes the space before the next glyph, unlike MeasureTextEx's bounding width.
inline float TextAdvance(Font font, std::string_view text, float size, float spacing = 1)
{
    if (font.baseSize <= 0 || !font.glyphs || !font.recs)
    {
        return 0;
    }
    const auto owned = std::string(text);
    float advance = 0;
    for (std::size_t i = 0; i < owned.size();)
    {
        int bytes = 0;
        const auto codepoint = GetCodepointNext(owned.c_str() + i, &bytes);
        const auto index = GetGlyphIndex(font, codepoint);
        const float glyph_width =
            font.glyphs[index].advanceX ? font.glyphs[index].advanceX : font.recs[index].width;
        advance += glyph_width * size / font.baseSize + spacing;
        i += bytes;
    }
    return advance;
}

inline void DrawLabel(std::string_view text, Vector2 position, float size, Color color)
{
    const auto owned = std::string(text);
    DrawTextEx(HintFont(), owned.c_str(), position, size, 1, color);
}

inline void DrawModifierIcon(std::uint8_t modifier, Vector2 position, Color color, float size = 8)
{
    const auto point = [&](float x, float y)
    {
        return Vector2{position.x + x * size / 8, position.y + y * size / 8};
    };
    const auto line = [&](float x, float y, float x2, float y2)
    {
        DrawLineV(point(x, y), point(x2, y2), color);
    };
    if (modifier == 1)
    {
        line(0, 4, 4, 0);
        line(4, 0, 8, 4);
        line(4, 0, 4, 8);
    }
    else if (modifier == 2)
    {
        line(0, 6, 4, 2);
        line(4, 2, 8, 6);
    }
    else if (modifier == 4)
    {
        line(0, 1, 2, 1);
        line(2, 1, 6, 7);
        line(6, 7, 8, 7);
        line(4, 1, 8, 1);
    }
    else
    {
        line(2, 2, 6, 2);
        line(6, 2, 6, 6);
        line(6, 6, 2, 6);
        line(2, 6, 2, 2);
        for (const auto center : {point(2, 2), point(6, 2), point(2, 6), point(6, 6)})
        {
            DrawCircleLinesV(center, size / 4, color);
        }
    }
}
