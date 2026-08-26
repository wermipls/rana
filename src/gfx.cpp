#include "gfx.hpp"
#include "gfx_colorspace.hpp"
#include "sdl_error.hpp"
#include "log.hpp"
#include <glad/glad.h>
#include "fs.hpp"
#include <SDL3/SDL_init.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>
#include <tracy/Tracy.hpp>
#include <stdexcept>

namespace rana {
namespace gfx {

static inline glm::vec4 srgb(glm::vec4 color)
{
    return glm::vec4(
        srgb_to_float(color.r) * color.a,
        srgb_to_float(color.g) * color.a,
        srgb_to_float(color.b) * color.a,
        color.a
    );
}

static inline glm::vec4 srgb(glm::vec3 color)
{
    return srgb(glm::vec4(color, 1.f));
}

void GLAPIENTRY opengl_msg_cb(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar* message, const void* /*user_param*/)
{
    const char *fmt = "GL: %s - %s: %s";
    const char *s_src = "unknown";
    const char *s_type = "unknown";
    switch (source) {
        case GL_DEBUG_SOURCE_API:               s_src = "api"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:     s_src = "window"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:   s_src = "shader"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:       s_src = "3rdparty"; break;
        case GL_DEBUG_SOURCE_APPLICATION:       s_src = "app"; break;
        case GL_DEBUG_SOURCE_OTHER:             s_src = "other"; break;
    }
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:               s_type = "err"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: s_type = "deprecated"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  s_type = "undefined"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         s_type = "portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         s_type = "performance"; break;
        case GL_DEBUG_TYPE_MARKER:              s_type = "marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          s_type = "push"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           s_type = "pop"; break;
        case GL_DEBUG_TYPE_OTHER:               s_type = "other"; break;
    }
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            log::err(fmt, s_src, s_type, message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
        case GL_DEBUG_SEVERITY_LOW:
            log::warn(fmt, s_src, s_type, message);
            break;
    }
}

unsigned int create_vao(float *vertices, size_t sz_vertices, unsigned int *indices, size_t sz_indices)
{
    unsigned int vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sz_vertices, vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    unsigned int ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sz_indices, indices, GL_STATIC_DRAW);

    return vao;
}

Context::Context(const char *title, int w, int h)
{
    // FIXME: needs proper error reporting.

    if (w <= 0 || h <= 0) {
        log::err("invalid width/height");
        abort();
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log::err("failed to initialize video subsystem: %s", SDL_GetError());
        abort();
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow(
        title,
        w,
        h,
        SDL_WINDOW_OPENGL
    );

    if (!window) {
        log::err("failed to create window: %s", SDL_GetError());
        abort();
    }

    glcontext = SDL_GL_CreateContext(window);
    if (!glcontext) {
        log::err("failed to create gl context: %s", SDL_GetError());
        abort();
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        log::err("failed to initialize glad");
        abort();
    }

    glDebugMessageCallback(opengl_msg_cb, nullptr);
    glEnable(GL_DEBUG_OUTPUT);

    glViewport(0, 0, w, h);

    glGenVertexArrays(1, &quad_vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindVertexArray(quad_vao);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4*0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4*2));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4*4));

    sprite_shader = Shader::fallback();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, glcontext);
    ImGui_ImplOpenGL3_Init();

    batch.vtxbuf.reserve(1024);

    setCanvasSize(w,h);
}

Context::~Context()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(glcontext);
    SDL_DestroyWindow(window);
}

void Context::clear(glm::vec3 color)
{
    color = srgb(color);
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Context::drawBegin()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    glEnable(GL_FRAMEBUFFER_SRGB);
    sprite_shader.use();

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    resetTransform();
}

void Context::drawFinish()
{
    if (auto sz = transform_stack.size(); sz != 0) {
        transform_stack.resize(0);
        log::warn("transform stack is non zero (size == %d); likely a missing pop somewhere", sz);
    }
    flush();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
}

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;

void Context::flush()
{
    if (batch.vtxbuf.size() == 0) {
        return;
    }

    sprite_shader.setUniform("Projection", projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, batch.texture);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, batch.vtxbuf.size() * sizeof(DrawBatch::Vert), batch.vtxbuf.data(), GL_STREAM_DRAW);

    glBindVertexArray(quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, batch.vtxbuf.size());

    batch.vtxbuf.resize(0);
}

void Context::drawTextureSub(Texture &tex, vec4 rect, vec2 pos, vec4 color)
{
    ZoneScoped;

    auto t = tex.texture();
    if (batch.texture != t) {
        flush();
        batch.texture = t;
    }

    auto r1 = vec2{rect.x, rect.y};
    auto size = vec2{rect.z, rect.w};
    auto r2 = r1 + size;

    auto model = transform;
    model = glm::translate(model, vec3(pos, 0.0f));
    model = glm::scale(model, vec3(size, 1.0f));

    auto &buf = batch.vtxbuf;
    buf.push_back({ model * vec4{0,1,0,1}, vec2{r1.x,r2.y} / tex.size(), color });
    buf.push_back({ model * vec4{1,0,0,1}, vec2{r2.x,r1.y} / tex.size(), color });
    buf.push_back({ model * vec4{0,0,0,1}, vec2{r1.x,r1.y} / tex.size(), color });
    buf.push_back({ model * vec4{0,1,0,1}, vec2{r1.x,r2.y} / tex.size(), color });
    buf.push_back({ model * vec4{1,1,0,1}, vec2{r2.x,r2.y} / tex.size(), color });
    buf.push_back({ model * vec4{1,0,0,1}, vec2{r2.x,r1.y} / tex.size(), color });
}

void Context::drawSprite(Texture &tex, vec2 pos, vec2 scale, float r, vec4 color)
{
    ZoneScoped;

    auto t = tex.texture();
    if (batch.texture != t) {
        flush();
        batch.texture = t;
    }

    color = srgb(color);

    auto model = transform;
    model = glm::translate(model, vec3(pos, 0.0f));
    model = glm::rotate(model, r, {0,0,1});
    model = glm::scale(model, vec3(tex.size() * scale, 1.0f));
    model = glm::translate(model, vec3{-0.5f, -0.5f, 0.0f});

    auto &buf = batch.vtxbuf;
    buf.push_back({ model * vec4{0,1,0,1}, {0,1}, color });
    buf.push_back({ model * vec4{1,0,0,1}, {1,0}, color });
    buf.push_back({ model * vec4{0,0,0,1}, {0,0}, color });
    buf.push_back({ model * vec4{0,1,0,1}, {0,1}, color });
    buf.push_back({ model * vec4{1,1,0,1}, {1,1}, color });
    buf.push_back({ model * vec4{1,0,0,1}, {1,0}, color });
}

// adapted from https://github.com/Photosounder/fopen_utf8
// fixme: put this somewhere else..
static int utf8_char_size(const char *c)
{
    const uint8_t m0x       = 0x80, c0x     = 0x00,
                  m10x      = 0xC0, c10x    = 0x80,
                  m110x     = 0xE0, c110x   = 0xC0,
                  m1110x    = 0xF0, c1110x  = 0xE0,
                  m11110x   = 0xF8, c11110x = 0xF0;

    if ((c[0] & m0x) == c0x)
        return 1;

    if ((c[0] & m110x) == c110x)
    if ((c[1] & m10x) == c10x)
        return 2;

    if ((c[0] & m1110x) == c1110x)
    if ((c[1] & m10x) == c10x)
    if ((c[2] & m10x) == c10x)
        return 3;

    if ((c[0] & m11110x) == c11110x)
    if ((c[1] & m10x) == c10x)
    if ((c[2] & m10x) == c10x)
    if ((c[3] & m10x) == c10x)
        return 4;

    if ((c[0] & m10x) == c10x) // not a first UTF-8 byte
        return 0;

    return -1; // if c[0] is a first byte but the other bytes don't match
}

static uint32_t utf8_to_unicode32(const char **s)
{
    const char *c = *s;
    uint32_t v;
    const uint8_t m6 = 63, m5 = 31, m4 = 15, m3 = 7;

    if (c==NULL)
        return 0;

    auto size = utf8_char_size(c);

    if (size > 0)
        *s += size-1;

    switch (size)
    {
        case 1:
            v = c[0];
            break;
        case 2:
            v = c[0] & m5;
            v = v << 6 | (c[1] & m6);
            break;
        case 3:
            v = c[0] & m4;
            v = v << 6 | (c[1] & m6);
            v = v << 6 | (c[2] & m6);
            break;
        case 4:
            v = c[0] & m3;
            v = v << 6 | (c[1] & m6);
            v = v << 6 | (c[2] & m6);
            v = v << 6 | (c[3] & m6);
            break;
        case 0:  // not a first UTF-8 byte
        case -1: // corrupt UTF-8 letter
        default:
            v = -1;
            break;
    }

    return v;
}

void Context::text(Font &font, const char *text, vec2 pos, vec4 color)
{
    color = srgb(color);

    float font_scale = font_size / font.sizePt();
    pushTransform();
    translate(pos);
    scale(vec2{font_scale});
    pos = {0,0};

    auto p = text;
    while (*p) {
        auto codepoint = utf8_to_unicode32(&p);
        p++;
        if (codepoint == '\n') {
            pos.x = 0;
            pos.y += font.lineHeight();
            continue;
        } else if (codepoint == '\t') {
            pos.x += font.getGlyph(' ').advance * 8;
            continue;
        }
        auto g = font.getGlyph(codepoint);
        // only the offset from the origin gets rounded, this is for two reasons:
        // 1) allow subpixel position AND keep text perfectly sharp w/ integer values
        // 2) avoid ugly snapping when doing subpixel movement
        drawTextureSub(font.texture(), {g.x0, g.y0, g.x1-g.x0, g.y1-g.y0}, glm::round(pos + g.off), color);
        pos.x += g.advance;
    }

    popTransform();
}

}
}
