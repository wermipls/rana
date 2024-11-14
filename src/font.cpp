#include "font.hpp"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include "fs.hpp"
#include <glad/glad.h>

namespace rana {
namespace gfx {

auto Font::load(const char *fn, float size, bool is_point_size) -> expected<Font, err>
{
    std::vector<uint8_t> font_file;
    if (!fs::readfile(font_file, fn)) {
        return unexpected("failed to read font file");
    }

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, font_file.data(), 0)) {
        return unexpected("failed to init font info");
    }

    stbtt_pack_context pc;
    int width = 2048;
    int height = 2048;
    std::vector<uint8_t> atlas(width*height);

    if (!stbtt_PackBegin(&pc, atlas.data(), width, height, 0, 1, nullptr)) {
        return unexpected("pack begin fail");
    }

    //stbtt_PackSetOversampling(&pc, oversample, oversample); FIXME: font is not scale aware

    // fixme: assumes a single range.. not customizable
    constexpr auto range_start = 31;
    constexpr auto range_end = 512;
    constexpr auto range_size = range_end - range_start;
    stbtt_packedchar range1[range_size];
    stbtt_pack_range range{};
    range.font_size = is_point_size ? STBTT_POINT_SIZE(size) : size;
    range.first_unicode_codepoint_in_range = range_start;
    range.num_chars = range_size;
    range.chardata_for_range = range1;

    if (!stbtt_PackFontRanges(&pc, font_file.data(), 0, &range, 1)) {
        stbtt_PackEnd(&pc);
        return unexpected("failed to pack font ranges");
    }
    stbtt_PackEnd(&pc);

    glyph_map glyphs;
    for (auto i = range_start; i < range_size; i++) {
        auto &s = range1[i-range_start];
        glyph glyph;

        glyph.x0 = s.x0;
        glyph.y0 = s.y0;
        glyph.x1 = s.x1;
        glyph.y1 = s.y1;
        glyph.off  = vec2{s.xoff,  s.yoff};
        glyph.off2 = vec2{s.xoff2, s.yoff2};
        glyph.advance = s.xadvance;

        glyphs[i] = glyph;
    }

    auto tex = Texture::loadBuffer(atlas.data(), width, height, 1);
    tex.setMinFilter(Texture::Filter::Linear);

    auto font = Font(tex, glyphs);

    int ascent, descent, linegap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &linegap);

    float scale;
    if (is_point_size) {
        scale = stbtt_ScaleForMappingEmToPixels(&info, size);
    } else {
        scale = stbtt_ScaleForPixelHeight(&info, size);
    }
    font._line_gap = linegap * scale;
    font._ascent = ascent * scale;
    font._descent = descent * scale;
    font._line_height = font._ascent - font._descent + font._line_gap;

    font.placeholder = font.glyphs[31]; // FIXME: hack
    return font;
}

}
}
