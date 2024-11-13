#pragma once
#include "types.hpp"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace rana {
namespace gfx {

class Texture {
    using vec2 = glm::vec2;
    using vec4 = glm::vec4;

    uint32_t tex;
    vec2 size_px;

    Texture(uint32_t tex, vec2 size) : tex{tex}, size_px{size} {};
public:
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(Texture&) = delete;
    Texture(Texture&& a) : tex{a.tex}, size_px{a.size_px} { a.tex = 0; };
    Texture& operator=(Texture&& a) { tex = a.tex; size_px = a.size_px; a.tex = 0; return *this; };

    static auto load(const char *fn) -> Texture;
    static auto fallback() -> Texture;

    constexpr auto texture() { return tex; }
    constexpr auto width() { return size_px.x; }
    constexpr auto height() { return size_px.y; }
    constexpr auto size() -> vec2 { return size_px; }
};

}
}
