#pragma once

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_render.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>
#include "shader.hpp"
#include "gfx_textures.hpp"
#include "font.hpp"

namespace rana {
namespace gfx {

constexpr auto gamma = 2.2f;
inline auto srgb(glm::vec4 color) { return glm::pow(color, {gamma, gamma, gamma, 1.f}); }
inline auto srgb(glm::vec3 color) { return glm::pow(color, {gamma, gamma, gamma}); }

struct DrawBatch {
    using vec2 = glm::vec2;
    using vec4 = glm::vec4;
    struct Vert {
        vec2 pos;
        vec2 uv;
        vec4 color;
    };

    std::vector<Vert> vtxbuf;
    uint32_t texture;
};

class Context {
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;
    using mat4 = glm::mat4;

    SDL_Window *window;
    SDL_GLContext glcontext;
    Shader sprite_shader;
    uint32_t quad_vao;
    uint32_t vbo;
    DrawBatch batch;
    mat4 transform;

    void flush();

public:
    Context(const char *title = "rana", int w = 1280, int h = 720);
    ~Context();

    void drawBegin();
    void drawFinish();

    void clear(vec3 color);
    void drawTextureSub(Texture &tex, vec4 rect, vec2 pos, vec4 color = {1,1,1,1});
    void drawSprite(Texture &tex, vec2 pos, vec2 scale = {1,1}, float r = 0, vec4 color = {1,1,1,1});
    void text(Font &font, const char *text, vec2 pos, vec4 color = {1,1,1,1});

    auto resetTransform()   { transform = mat4(1.0f); }
    auto translate(vec2 v)  { transform = glm::translate(transform, vec3(v, 0.f)); }
    auto rotate(float r)    { transform = glm::rotate(transform, r, {0,0,1}); }
    auto scale(vec2 v)      { transform = glm::scale(transform, vec3{v, 1.f}); }
};

}
}
