#include "gfx.hpp"
#include "sdl_error.hpp"
#include "log.hpp"
#include <glad/glad.h>
#include "fs.hpp"

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
    auto vertexshader = load_shader("shaders/vs.glsl", ShaderType::Vertex);
    auto fragshader = load_shader("shaders/fs.glsl", ShaderType::Fragment);

    auto shaderprog = glCreateProgram();
    glAttachShader(shaderprog, vertexshader);
    glAttachShader(shaderprog, fragshader);
    glLinkProgram(shaderprog);

    int success;
    glGetProgramiv(shaderprog, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(shaderprog, sizeof(info), nullptr, info);
        log::err("failed to link shader program:\n%s", info);
    }

    glDeleteShader(vertexshader);
    glDeleteShader(fragshader);

    return shaderprog;
}

Context::Context(const char *title, int w, int h)
{
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
         0.0f,  0.5f, 0.0f,
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    shaderprog = load_shader_program("shaders/fs.glsl", "shaders/vs.glsl");
}

Context::~Context()
{
    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
}

void Context::draw()
{
    glClearColor(0.2f, 0.0f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderprog);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    SDL_GL_SwapWindow(window);
}

}
}
