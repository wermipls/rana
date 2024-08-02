#pragma once
#include <glm/vec2.hpp>
#include <unordered_map>
#include <vector>

namespace rana {
namespace input {

enum class JoyInput : int {
    LStickLeft,
    LStickRight,
    LStickUp,
    LStickDown,
    RStickLeft,
    RStickRight,
    RStickUp,
    RStickDown,
    DLeft,
    DRight,
    DUp,
    DDown,
    West,
    East,
    North,
    South,
    LTrigger,
    RTrigger,
    LShoulder,
    RShoulder,
    LStick,
    RStick,
    Start,
};

struct Mapping {
    int scancode;
    JoyInput joy_input;
    float dz;
    float dz_outer;
    float magnitude_curve;
    float threshold;
    // state
    float input_raw = 0;
    float input = 0;
    int held_for_ticks = -2;
};

struct Axis {
    int negative;
    int positive;
    float input_raw = 0;
    float input = 0;
};

struct Vector {
    int negative_x, positive_x, negative_y, positive_y;
    float dz;
    float dz_outer;
    float magnitude_curve;
    // state
    glm::vec2 input_raw = {0,0};
    glm::vec2 input_processed = {0,0};
};

class Mapper {
public:
    std::unordered_map<int, int> id2mapping;
    std::unordered_map<int, int> id2axis;
    std::unordered_map<int, int> id2vector;
    std::vector<Mapping> mappings;
    std::vector<Axis> axes;
    std::vector<Vector> vectors;

    float init_dz;
    float init_dz_outer;
    float init_magnitude_curve;
    float init_threshold;

    Mapper(float dz = 0.1f, float dz_outer = 0.1f, float curve = 1.0f, float threshold = 0.5f)
      : init_dz{dz},
        init_dz_outer{dz_outer},
        init_magnitude_curve{curve},
        init_threshold{threshold} {
        
    }
    void addMapping(int id_mapping, JoyInput joy_input, int kb_scancode);
    void addAxis(int id_axis, int negative, int positive);
    void addVector(int id_vector, int negative_x, int positive_x, int negative_y, int positive_y);

    // called every tick
    void update(void *gamepad_handle);

    bool pressed(int id_mapping);
    bool released(int id_mapping);
    bool held(int id_mapping);
    bool repeated(int id_mapping, int delay, int interval);
    float analog(int id_mapping);
    // returns a normalized axis value
    float axis(int id_axis);
    // returns a normalized vector
    glm::vec2 vector(int id_vector);
};

}
}