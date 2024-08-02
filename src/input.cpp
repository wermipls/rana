#include "input.hpp"
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <cmath>

namespace rana {
namespace input {

void Mapper::addMapping(int id_mapping, JoyInput joy_input, int kb_scancode)
{
    Mapping mapping{};
    mapping.dz = init_dz;
    mapping.dz_outer = init_dz_outer;
    mapping.magnitude_curve = init_magnitude_curve;
    mapping.threshold = init_threshold;
    mapping.scancode = kb_scancode;
    mapping.joy_input = joy_input;

    id2mapping[id_mapping] = mappings.size();
    mappings.push_back(mapping);
}

void Mapper::addAxis(int id_axis, int negative, int positive)
{
    Axis axis{};
    axis.negative = negative;
    axis.positive = positive;

    id2mapping[id_axis] = axes.size();
    axes.push_back(axis);
}

void Mapper::addVector(int id_vector, int negative_x, int positive_x, int negative_y, int positive_y)
{
    Vector vector{};
    vector.negative_x = negative_x;
    vector.positive_x = positive_x;
    vector.negative_y = negative_y;
    vector.positive_y = positive_y;

    vector.dz = init_dz;
    vector.dz_outer = init_dz_outer;
    vector.magnitude_curve = init_magnitude_curve;

    id2mapping[id_vector] = vectors.size();
    vectors.push_back(vector);
}

static float get_gamepad_axis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis, bool is_negative)
{
    auto state = SDL_GetGamepadAxis(gamepad, axis);
    if (is_negative) {
        return std::min(state, (int16_t)0) / -32768.0f;
    } else {
        return std::max(state, (int16_t)0) / 32767.0f;
    }
}

static float get_gamepad_state(SDL_Gamepad *gamepad, JoyInput &input)
{
    if (!gamepad) return 0;

    using enum JoyInput;
    switch (input) {
        case LStickLeft:  return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_LEFTX, 1);
        case LStickRight: return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_LEFTX, 0);
        case LStickUp:    return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_LEFTY, 1);
        case LStickDown:  return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_LEFTY, 0);
        case RStickLeft:  return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX, 1);
        case RStickRight: return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX, 0);
        case RStickUp:    return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY, 1);
        case RStickDown:  return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY, 0);
        case DLeft:       return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        case DRight:      return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        case DUp:         return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
        case DDown:       return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        case West:        return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST);
        case East:        return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST);
        case North:       return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH);
        case South:       return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
        case LTrigger:    return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0);
        case RTrigger:    return get_gamepad_axis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0);
        case LShoulder:   return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        case RShoulder:   return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        case LStick:      return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
        case RStick:      return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        case Start:       return SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
    }
    return 0;
}

static float handle_dz(float input, float inner, float outer)
{
    auto factor = std::max(0.01f, 1.0f - outer - inner);
    return std::min(1.0f, std::max(0.f, (input - inner) / factor));
}

void Mapper::update(void *gamepad_handle)
{
    auto kbstate = SDL_GetKeyboardState(0);
    for (auto &m : mappings) {
        int kbinput = kbstate[m.scancode];
        m.input_raw = get_gamepad_state((SDL_Gamepad *)gamepad_handle, m.joy_input);
        if (kbinput) m.input_raw = 1;

        m.input = std::pow(handle_dz(m.input_raw, m.dz, m.dz_outer), m.magnitude_curve);
        auto held = m.input >= m.threshold;

        // fixme: handle wrapping
        if (held) {
            if (m.held_for_ticks < 0) m.held_for_ticks = 0;
            m.held_for_ticks++;
        } else {
            if (m.held_for_ticks > 0) m.held_for_ticks = 0;
            m.held_for_ticks--;
        }
    }
}

bool Mapper::pressed(int id_mapping)
{
    return mappings[id2mapping[id_mapping]].held_for_ticks == 1;
}

bool Mapper::released(int id_mapping)
{
    return mappings[id2mapping[id_mapping]].held_for_ticks == -1;
}

bool Mapper::held(int id_mapping)
{
    return mappings[id2mapping[id_mapping]].held_for_ticks > 0;
}

bool Mapper::repeated(int id_mapping, int delay, int interval)
{
    auto &m = mappings[id2mapping[id_mapping]];
    if (m.held_for_ticks == 1) return true;
    if (m.held_for_ticks < delay) return false;
    return ((m.held_for_ticks - delay) % interval) == 0;
}
float Mapper::analog(int id_mapping)
{
    return mappings[id2mapping[id_mapping]].input;
}

}
}
