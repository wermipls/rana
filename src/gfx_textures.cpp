#include "gfx_textures.hpp"
#include "fs.hpp"
#include "log.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_MAX_DIMENSIONS 8192
#include "stb_image.h"

#include <glad/glad.h>
#include "error_handling.hpp"

namespace rana {
namespace gfx {

static uint32_t generate_texture_from_buffer(uint8_t *buf, int w, int h, int ch)
{
    uint32_t internal_format;
    uint32_t format;
    switch (ch) {
        case 4: internal_format = GL_SRGB_ALPHA; format = GL_RGBA; break;
        case 3: internal_format = GL_SRGB;       format = GL_RGB; break;
        case 1: internal_format = GL_RED;        format = GL_RED; break;
        default:
            throw Exception("unsupported texture channel count: " + std::to_string(ch));
    }

    uint32_t tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, GL_UNSIGNED_BYTE, buf);
    if (ch == 1) {
        int swizzle[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // fixme: insert custom mipmap gen here
    glGenerateMipmap(GL_TEXTURE_2D);

    return tex;
}

auto Texture::fallback() -> Texture
{
    static constexpr auto size = 64;
    uint32_t data[size][size];
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            data[x][y] = ((x / (size/2)) != (y / (size/2))) ? 0xFF8800FF : 0xFF88FF00;
        }
    }

    auto tex = generate_texture_from_buffer((uint8_t *)data, size, size, 4);

    return Texture(tex, vec2{size, size});
}

auto Texture::load(const char *fn) -> Texture
{
    std::vector<uint8_t> file;
    if (!fs::readfile(file, fn)) {
        return fallback();
    }

    int w, h, ch;
    auto *data = stbi_load_from_memory(file.data(), file.size(), &w, &h, &ch, 4);
    if (!data) {
        log::err("failed to load texture '%s': %s", fn, stbi_failure_reason());
        return fallback();
    }

    auto tex = generate_texture_from_buffer(data, w, h, 4);

    stbi_image_free(data);

    return Texture(tex, {w, h});
}

auto Texture::loadBuffer(uint8_t *buf, int w, int h, int ch) -> Texture
{
    auto tex = generate_texture_from_buffer(buf, w, h, ch);

    return Texture(tex, {w, h});
}

auto Texture::setMagFilter(Filter f) -> void
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)f);
}

auto Texture::setMinFilter(Filter f) -> void
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)f);
}

Texture::~Texture()
{
    if (tex) {
        glDeleteTextures(1, &tex);
    }
}

}
}
