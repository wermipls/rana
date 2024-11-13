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

namespace rana {
namespace gfx {

uint32_t create_fallback_texture()
{
    static constexpr auto size = 64;
    uint32_t data[size][size];
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            data[x][y] = ((x / (size/2)) != (y / (size/2))) ? 0xFFFF00FF : 0xFF000000;
        }
    }

    uint32_t tex; 
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return tex;
}

uint32_t load_texture(const char *fn)
{
    std::vector<uint8_t> file;
    if (!fs::readfile(file, fn)) {
        return create_fallback_texture();
    }

    int w, h, ch;
    auto *data = stbi_load_from_memory(file.data(), file.size(), &w, &h, &ch, 4);
    if (!data) {
        log::err("failed to load texture '%s': %s", fn, stbi_failure_reason());
        return create_fallback_texture();
    }

    uint32_t tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    return tex;
}

void destroy_texture(uint32_t texture)
{
    glDeleteTextures(1, &texture);
}

}
}
