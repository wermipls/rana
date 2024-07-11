#include "gfx.hpp"
#include "sdl_error.hpp"
#include "log.hpp"
#include <glad/glad.h>
#include "fs.hpp"
#include <SDL3/SDL_init.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

namespace rana {
namespace gfx {

enum ShaderType {
    Fragment,
    Vertex
};

static unsigned int load_shader(const char *fn, ShaderType type)
{
    unsigned int gltype;
    switch (type) {
        case ShaderType::Fragment: gltype = GL_FRAGMENT_SHADER; break;
        case ShaderType::Vertex:   gltype = GL_VERTEX_SHADER; break;
    }
    unsigned int id = glCreateShader(gltype);

    std::vector<uint8_t> source;
    if (!fs::readfile(source, fn)) {
        log::err("failed to load shader file");
        source.resize(1);
    }

    auto src = (char *)source.data();
    int src_size = source.size();
    glShaderSource(id, 1, &src, &src_size);

    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(id, sizeof(info), nullptr, info);
        log::err("failed to compile shader '%s':\n%s", fn, info);
    }

    return id;
}

unsigned int load_shader_program(const char *fn_fs, const char *fn_vs)
{
    auto vs = load_shader(fn_vs, ShaderType::Vertex);
    auto fs = load_shader(fn_fs, ShaderType::Fragment);

    auto shaderprog = glCreateProgram();
    glAttachShader(shaderprog, vs);
    glAttachShader(shaderprog, fs);
    glLinkProgram(shaderprog);

    int success;
    glGetProgramiv(shaderprog, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(shaderprog, sizeof(info), nullptr, info);
        log::err("failed to link shader program:\n%s", info);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return shaderprog;
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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    unsigned int ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sz_indices, indices, GL_STATIC_DRAW);

    return vao;
}

static unsigned int vao2;
static unsigned int sh2;

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
        sdl_error("failed to create window");
        throw;
    }

    glcontext = SDL_GL_CreateContext(window);
    if (!glcontext) {
        sdl_error("failed to create gl context");
        throw;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        log::err("failed to initialize glad");
        throw;
    }

    glViewport(0, 0, w, h);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
    };

    unsigned int indices[] = {
        0, 1, 2,
        1, 2, 3,
    };

    vao = create_vao(vertices, sizeof(vertices), indices, sizeof(indices));
    for (int i = 0; i < 12; i++) {
        vertices[i] += 0.25;
    }
    vao2 = create_vao(vertices, sizeof(vertices), indices, sizeof(indices));

    shaderprog = load_shader_program("shaders/fs.glsl", "shaders/vs.glsl");
    sh2 = load_shader_program("shaders/fs2.glsl", "shaders/vs.glsl");

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines;

    ImFontConfig cfg;
    cfg.PixelSnapH = true;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);

    ImGuiStyle &style = ImGui::GetStyle();
    style.AntiAliasedLinesUseTex = false;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, glcontext);
    ImGui_ImplOpenGL3_Init();
}

Context::~Context()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
}

void Context::draw()
{
    glClearColor(0.2f, 0.0f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderprog);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glUseProgram(sh2);
    glBindVertexArray(vao2);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
}

}
}
