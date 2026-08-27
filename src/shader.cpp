#include "shader.hpp"
#include <string.h>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include "fs.hpp"
#include "log.hpp"

namespace rana {
namespace gfx {

enum ShaderType {
    Fragment = GL_FRAGMENT_SHADER,
    Vertex = GL_VERTEX_SHADER,
};

static const char default_vs[] =R"(
#version 330 core
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Color;
uniform mat4 Projection;
out vec2 Frag_UV;
out vec4 Frag_Color;
void main()
{
    Frag_UV = UV;
    Frag_Color = Color;
    gl_Position = Projection * vec4(Position.xy,0,1);
}
)";

static const char default_fs[] =R"(
#version 330 core
in vec2 Frag_UV;
in vec4 Frag_Color;
uniform sampler2D Texture;
layout (location = 0) out vec4 Out_Color;
void main()
{
    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
}
)";

// FIXME: code duplication
static auto load_shader(const char *fn, ShaderType type) -> tl::expected<uint32_t, std::string>
{
    std::vector<uint8_t> source;
    if (!fs::readfile(source, fn)) {
        return tl::unexpected("failed to load shader file");
    }

    auto id = glCreateShader(type);

    auto src = (char *)source.data();
    int src_size = source.size();
    glShaderSource(id, 1, &src, &src_size);

    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(id, sizeof(info), nullptr, info);
        glDeleteShader(id);
        return tl::unexpected(std::string("failed to compile shader:\n") + info);
    }

    return id;
}

static auto load_shader_string(const char *shader, ShaderType type) -> tl::expected<uint32_t, std::string>
{
    auto id = glCreateShader(type);

    int sz = strlen(shader);
    glShaderSource(id, 1, &shader, &sz);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(id, sizeof(info), nullptr, info);
        glDeleteShader(id);
        return tl::unexpected(std::string("failed to compile shader:\n") + info);
    }

    return id;
}

static auto load_shader_program(uint32_t vs, uint32_t fs) -> tl::expected<uint32_t, std::string>
{
    auto shaderprog = glCreateProgram();
    glAttachShader(shaderprog, vs);
    glAttachShader(shaderprog, fs);
    glLinkProgram(shaderprog);

    int success;
    glGetProgramiv(shaderprog, GL_LINK_STATUS, &success);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(shaderprog, sizeof(info), nullptr, info);
        glDeleteProgram(shaderprog);
        return tl::unexpected(std::string("failed to link shader program: ") + info);
    }

    return shaderprog;
}

static auto load_shader_program(const char *fn_vs, const char *fn_fs) -> tl::expected<uint32_t, std::string>
{
    auto vs = load_shader(fn_vs, ShaderType::Vertex);
    if (!vs) {
        return tl::unexpected(std::string("vertex shader: ") + vs.error());
    }
    auto fs = load_shader(fn_fs, ShaderType::Fragment);
    if (!fs) {
        glDeleteShader(*vs);
        return tl::unexpected(std::string("fragment shader: ") + vs.error());
    }

    return load_shader_program(*vs, *fs);
}

auto Shader::fromString(const char *vs_str, const char *fs_str) -> tl::expected<Shader, std::string>
{
    auto vs = load_shader_string(vs_str, ShaderType::Vertex);
    if (!vs) {
        return tl::unexpected(std::string("vertex shader: ") + vs.error());
    }
    auto fs = load_shader_string(fs_str, ShaderType::Fragment);
    if (!fs) {
        glDeleteShader(*vs);
        return tl::unexpected(std::string("fragment shader: ") + vs.error());
    }

    auto program = load_shader_program(*vs, *fs);
    if (!program) {
        return tl::unexpected(program.error());
    }

    return Shader(*program);
}

auto Shader::fromFile(const char *fn_vs, const char *fn_fs) -> tl::expected<Shader, std::string>
{
    auto program = load_shader_program(fn_vs, fn_fs);
    if (!program) {
        return tl::unexpected(program.error());
    }

    return Shader(*program);
}

auto Shader::fallback() -> Shader
{
    auto shader = fromString(default_vs, default_fs);
    if (shader) {
        return std::move(*shader);
    } else {
        log::err(shader.error().c_str());
        return Shader();
    }
}

// fixme: replace this with something better..
// seems very dumb to figure out the location by string everytime
auto Shader::setUniform(const char *name, mat4 value) -> void
{
    auto uniform = glGetUniformLocation(program_id, name);
    glUniformMatrix4fv(uniform, 1, GL_FALSE, glm::value_ptr(value));
}

auto Shader::use() -> void
{
    glUseProgram(program_id);
}

Shader::~Shader()
{
    if (program_id) {
        glDeleteProgram(program_id);
    }
}

}
}