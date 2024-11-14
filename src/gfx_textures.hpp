#pragma once
#include "types.hpp"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace rana {
namespace gfx {

class Texture {
public:
    enum class Filter : int {
        Nearest              = 0x2600,
        Linear               = 0x2601,
        LinearMipmap         = 0x2703, // this one is GL_LINEAR_MIPMAP_LINEAR
        NearestMipmapNearest = 0x2700, // those three are other,
        LinearMipmapNearest  = 0x2701, // nonsensical combinations.
        NearestMipmapLinear  = 0x2702, // why would you want those?
    };
    using enum Filter;
    using vec2 = glm::vec2;
    using vec4 = glm::vec4;

private:
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
    static auto loadBuffer(uint8_t *buf, int w, int h, int ch) -> Texture;
    static auto fallback() -> Texture;

    auto setMagFilter(Filter f) -> void;
    auto setMinFilter(Filter f) -> void;
    auto setFilter(Filter f) { setMagFilter(f); setMinFilter(f); };

    constexpr auto texture() { return tex; }
    constexpr auto width() { return size_px.x; }
    constexpr auto height() { return size_px.y; }
    constexpr auto size() -> vec2 { return size_px; }
};

}
}
