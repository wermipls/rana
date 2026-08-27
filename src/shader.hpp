#pragma once

#include <glm/mat4x4.hpp>
#include "error_handling.hpp"

namespace rana {
namespace gfx {

class Shader {
    using mat4 = glm::mat4;

    uint32_t program_id;

public:
    Shader(uint32_t program_id = 0) : program_id{program_id}
    {
    }
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(Shader&) = delete;
    Shader(Shader&& a) : program_id{a.program_id} { a.program_id = 0; };
    Shader& operator=(Shader&& a) { program_id = a.program_id; a.program_id = 0; return *this; };

    static auto fromString(const char *vs_str, const char *fs_str) -> tl::expected<Shader, std::string>;
    static auto fromFile(const char *fn_vs, const char *fn_fs) -> tl::expected<Shader, std::string>;
    static auto fallback() -> Shader;

    auto setUniform(const char *name, mat4 value) -> void;

    auto use() -> void;
};

}
}
