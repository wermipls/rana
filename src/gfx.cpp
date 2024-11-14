#include "gfx.hpp"
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
    SDL_Init(SDL_INIT_VIDEO);

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
        throw std::runtime_error("failed to create window");
    }

    glcontext = SDL_GL_CreateContext(window);
    if (!glcontext) {
        throw std::runtime_error("failed to create gl context");
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        throw std::runtime_error("failed to initialize glad");
    }

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
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Context::drawBegin()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    sprite_shader.use();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    resetTransform();
}

void Context::drawFinish()
{
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

    auto projection = glm::ortho(0.0f, 1280.0f, 720.0f, 0.0f);
    sprite_shader.setUniform("Projection", projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, batch.texture);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, batch.vtxbuf.size() * sizeof(DrawBatch::Vert), batch.vtxbuf.data(), GL_STREAM_DRAW);

    glBindVertexArray(quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, batch.vtxbuf.size());

    batch.vtxbuf.resize(0);
}

void Context::drawSprite(Texture &tex, vec2 pos, vec2 scale, float r, vec4 color)
{
    ZoneScoped;

    auto t = tex.texture();
    if (batch.texture != t) {
        flush();
        batch.texture = t;
    }

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

}
}
