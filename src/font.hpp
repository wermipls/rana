#pragma once

#include "types.hpp"
#include "error_handling.hpp"
#include "gfx_textures.hpp"
#include <unordered_map>

namespace rana {
namespace gfx {

class Font {
    using vec2 = glm::vec2;

    struct glyph {
        uint16_t x0, y0, x1, y1;
        vec2 off;
        vec2 off2;
        float advance;
    };
    using glyph_map = std::unordered_map<uint32_t, glyph>;

    glyph_map glyphs;
    glyph placeholder;
    Texture tex;

    float _line_gap;
    float _line_height;
    float _ascent;
    float _descent;
    float _size_pt;

    Font(Texture &t, glyph_map glyphs) : glyphs{glyphs}, tex{std::move(t)} { }
public:

    static auto load(const char *fn, float size_pt) -> tl::expected<Font, std::string>;

    constexpr auto &texture() { return tex; }
    constexpr auto lineGap() { return _line_gap; }
    constexpr auto lineHeight() { return _line_height; }
    constexpr auto ascent() { return _ascent; }
    constexpr auto descent() { return _descent; }
    constexpr auto sizePt() { return _size_pt; }

    inline auto getGlyph(uint32_t cp)
    {
        auto g = glyphs.find(cp);
        return (g == glyphs.end()) ? placeholder : g->second;
    }
};

}
}