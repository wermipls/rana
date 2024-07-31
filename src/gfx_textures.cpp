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

uint32_t load_texture(const char *fn)
{
    std::vector<uint8_t> file;
    if (!fs::readfile(file, fn)) {
        return 0;
    }

    int w, h, ch;
    auto *data = stbi_load_from_memory(file.data(), file.size(), &w, &h, &ch, 4);
    if (!data) {
        log::err("failed to load texture '%s': %s", fn, stbi_failure_reason());
        return 0;
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
