#include <stdint.h>
#include <assert.h>
#include "xutf8.h"
#include "textrenderutils.h"

float 
calculateStringRenderWidth(Font font, int font_size,
                           const char *str, size_t len)
{
    if (font.texture.id == 0) 
        font = GetFontDefault();

    float scale = (float) font_size / font.baseSize;
    float  w = 0;
    size_t i = 0;
    while (i < len) {

        int consumed;
        int letter = GetCodepoint(str + i, &consumed);
        assert(letter != '\n');

        int glyph_index = GetGlyphIndex(font, letter);

        if (letter == 0x3f)
            i++;
        else
            i += consumed;

        float delta;
        int advanceX = font.glyphs[glyph_index].advanceX;
        if (advanceX)
            delta = (float) advanceX * scale;
        else
            delta = (float) font.recs[glyph_index].width * scale
                  + (float) font.glyphs[glyph_index].offsetX * scale;

        w += delta;
    }
    return w;
}

size_t 
longestSubstringThatRendersInLessPixelsThan(Font font, int font_size,
                                            const char *str, size_t len, 
                                            float max_px_len)
{
    if (str == NULL)
        str = "";

    if (font.texture.id == 0) 
        font = GetFontDefault();

    float scale = (float) font_size / font.baseSize;

    float  w = 0;
    size_t i = 0;
    while (i < len) {

        uint32_t codepoint;
        int consumed = xutf8_sequence_to_utf32_codepoint(str + i, len - i, &codepoint);

        if (consumed < 0) {
            codepoint = '?';
            consumed = 1;
        }
        i += consumed;
        int glyph_index = GetGlyphIndex(font, codepoint);

        float delta;
        int advanceX = font.glyphs[glyph_index].advanceX;
        if (advanceX)
            delta = (float) advanceX * scale;
        else
            delta = (float) font.recs[glyph_index].width * scale
                  + (float) font.glyphs[glyph_index].offsetX * scale;
        
        assert(delta >= 0);
        if (w + delta > max_px_len)
            break;

        w += delta;
    }
    assert(i <= len);
    return i;
}

float renderString(Font font, const char *str, size_t len,
                   int off_x, int off_y, float font_size, 
                   Color tint)
{
    if (font.texture.id == 0) 
        font = GetFontDefault();

    int   y = off_y;
    float x = off_x; // Offset X to next character to draw

    float spacing = 0;
    float scale = (float) font_size / font.baseSize; // Character quad scaling factor

    size_t i = 0;
    while (i < len) {

        uint32_t codepoint;
        int consumed = xutf8_sequence_to_utf32_codepoint(str + i, len - i, &codepoint);

        if (consumed < 1) {
            codepoint = '?';
            consumed = 1;
        }

        assert(consumed > 0);
        int glyph_index = GetGlyphIndex(font, codepoint);

        assert(codepoint != '\n');

        if (codepoint != ' ' && codepoint != '\t') {
            Vector2 position = {x, y};
            DrawTextCodepoint(font, codepoint, position, 
                              font_size, tint);
        }

        float delta;
        int advance_x = font.glyphs[glyph_index].advanceX;
        if (advance_x == 0) 
            delta = (float) font.recs[glyph_index].width * scale + spacing;
        else 
            delta = (float) advance_x * scale + spacing;

        x += delta;
        i += consumed;
    }
    float w = x - (float) off_x;
    return w;
}
