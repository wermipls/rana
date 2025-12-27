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

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <glad/glad.h>
#include <tracy/Tracy.hpp>
#include "error_handling.hpp"

#ifdef RANA_ENABLE_JXL
    #include <jxl/decode_cxx.h>
#endif

namespace rana {
namespace gfx {

static uint32_t generate_texture_from_buffer(uint8_t *buf, int w, int h, int ch)
{
    ZoneScoped;

    uint32_t internal_format;
    uint32_t format;
    switch (ch) {
        case 4: internal_format = GL_SRGB_ALPHA; format = GL_RGBA; break;
        case 3: internal_format = GL_SRGB;       format = GL_RGB; break;
        case 1: internal_format = GL_RED;        format = GL_RED; break;
        default:
            throw Exception("unsupported texture channel count: " + std::to_string(ch));
    }

    if (ch == 4) {
        // premultiply the alpha.
        auto p = buf;
        for (size_t i = 0; i < w*h; i++) {
            auto alpha = p[3] / 255.f;
            p[0] = roundf(p[0] * alpha);
            p[1] = roundf(p[1] * alpha);
            p[2] = roundf(p[2] * alpha);
            p += 4;
        }
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
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16); // FIXME

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if ((ch != 3) && (ch != 4)) {
        glGenerateMipmap(GL_TEXTURE_2D);
        return tex;
    }

    int w_new = w;
    int h_new = h;
    int w_prev;
    int h_prev;
    unsigned char *s = nullptr;
    //unsigned char *prev_buf = nullptr;
    int lv;
    for (lv = 1; ; lv++) {
        w_prev = w_new;
        h_prev = h_new;
        w_new /= 2;
        h_new /= 2;
        if (!(w_new && h_new)) {
            break;
        }

        auto newbuf = stbir_resize_uint8_srgb(s ? s : buf, w_prev, h_prev, w_prev*ch, s, w_new, h_new, w_new*ch,
                                              ch == 4 ? STBIR_RGBA_PM : STBIR_RGB);
        if (!newbuf) {
            break;
        }

        s = newbuf;
        glTexImage2D(
            GL_TEXTURE_2D, lv, internal_format, w_new, h_new, 0, format, GL_UNSIGNED_BYTE, s
        );
    }
    STBIR_FREE(s, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, lv-1);

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

#ifdef RANA_ENABLE_JXL
    constexpr uint8_t jxl_magic[2] = {0xFF, 0x0A};
    if (memcmp(file.data(), jxl_magic, sizeof(jxl_magic)) == 0) {
        // try jpeg xl.
        auto dec = JxlDecoderMake(nullptr);

        if (JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS) {
            log::err("jxl: failed to subscribe to events");
            return fallback();
        }

        JxlDecoderSetInput(dec.get(), file.data(), file.size());
        JxlDecoderCloseInput(dec.get());

        std::vector<uint8_t> buf;
        auto fmt = JxlPixelFormat{4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
        JxlBasicInfo info;

        for (;;) {
            auto status = JxlDecoderProcessInput(dec.get());
            switch (status) {
                case JXL_DEC_ERROR: {
                    log::err("jxl: dec fail");
                    return fallback();
                }
                case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
                    size_t bufsize;
                    if (JxlDecoderImageOutBufferSize(dec.get(), &fmt, &bufsize) == JXL_DEC_ERROR) {
                        log::err("jxl: failed to get image out buffer size");
                        return fallback();
                    }

                    buf.resize(bufsize);
                    if (JxlDecoderSetImageOutBuffer(dec.get(), &fmt, buf.data(), bufsize) == JXL_DEC_ERROR) {
                        log::err("jxl: buf size set fail");
                        return fallback();
                    }
                    break;
                }
                case JXL_DEC_SUCCESS: {
                    if (JxlDecoderGetBasicInfo(dec.get(), &info) == JXL_DEC_ERROR) {
                        log::err("jxl: basic info get fail");
                        return fallback();
                    }
                    if (buf.size() == 0) {
                        log::err("jxl: success status but no decoded image in the buffer");
                        return fallback();
                    }
                    auto tex = generate_texture_from_buffer(buf.data(), info.xsize, info.ysize, fmt.num_channels);
                    return Texture(tex, {info.xsize, info.ysize});
                }
            }
        }
    }
#endif

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
