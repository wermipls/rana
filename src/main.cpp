#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include "gfx.hpp"
#include "gfx_textures.hpp"
#include "font.hpp"
#include "audio/backend.hpp"
#include "music/player.hpp"
#include "serializer.hpp"
#include "log.hpp"
#include "fs.hpp"
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <tracy/Tracy.hpp>
#include <glm/vec4.hpp>
#include <glm/ext.hpp>
#include "input.hpp"
#include "audio/audio.hpp"
#define SOL_ALL_SAFETIES_ON 1
#define SOL_EXCEPTIONS_ALWAYS_UNSAFE 1
#include <sol/sol.hpp>

void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    FrameMarkStart("Audio processing");
    auto player = (rana::audio::MusicPlayer *)userdata;
    while (additional_amount > 0) {
        auto samples = player->getSamples(1);
        auto bytes = samples.size() * 8;
        SDL_PutAudioStreamData(stream, samples.data(), bytes);
        additional_amount -= bytes;
    }
    FrameMarkEnd("Audio processing");
}

void SDLCALL audio_callback_new(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    using namespace rana::audio;

    FrameMarkStart("Audio processing");
    auto audio = (rana::audio::Context *)userdata;
    std::vector<SampleStereo> buf((additional_amount+7) / 8);

    audio->process(buf.data(), buf.size());
    SDL_PutAudioStreamData(stream, buf.data(), buf.size() * 8);

    FrameMarkEnd("Audio processing");
}

enum Inputs : int {
    Left,
    Right,
    Jump,
};

void synchronize_fps(double target_ticks_frame)
{
    static uint64_t ticks_next = 0;
    static double error = 0;
    int ticks_frame_flr = std::floor(target_ticks_frame);
    auto ticks = SDL_GetTicks();

    if (ticks_next < ticks - ticks_frame_flr - 1) {
        ticks_next = ticks;
        return;
    }

    if (ticks_next > ticks) {
        int delay = ticks_next - ticks;
        SDL_Delay(delay);
    }

    error += target_ticks_frame - (double)ticks_frame_flr;
    ticks_next += ticks_frame_flr;
    if (error >= 1) {
        error -= 1;
        ticks_next += 1;
    }
}

void enumerate_controllers()
{
    SDL_Init(SDL_INIT_GAMEPAD);
}

void module_path_from_name(std::string &name)
{
    for (auto &c : name) {
        if (c == '.') {
            c = '/';
        }
    }
    name += ".lua";
}

int fs_loader(lua_State* L)
{
    auto name = sol::stack::get<std::string>(L, 1);
    module_path_from_name(name);

    if (rana::fs::exists(name.c_str())) {
        std::vector<uint8_t> script;
        if (rana::fs::readfile(script, name.c_str())) {
            luaL_loadbuffer(L, (const char *)script.data(), script.size(), ("="+name).c_str());
            return 1;
        }
    } else {
        rana::log::err("module '%s' does not exist", name.c_str());
    }
    return 0;
}

sol::table open_fs(sol::this_state s)
{
    sol::state_view lua(s);

    sol::table fs = lua.create_table();
    fs["read"] = [lua](const char *fn) {
        std::vector<uint8_t> data;
        auto ok = rana::fs::readfile(data, fn);
        if (ok) {
            std::string str(data.begin(), data.end());
            return sol::make_object(lua, str);
        } else {
            return sol::make_object(lua, sol::nil);
        }
    };

    return fs;
}

sol::table open_log(sol::this_state s)
{
    sol::state_view lua(s);

    sol::table log = lua.create_table();
    log["err"]  = [](const char *msg) { rana::log::err("%s", msg); };
    log["warn"] = [](const char *msg) { rana::log::warn("%s", msg); };
    log["info"] = [](const char *msg) { rana::log::info("%s", msg); };

    return log;
}

int rana_main(int argc, char **argv)
{
    if (argc == 0) {
        return -1;
    }

    rana::fs::init(argv[0]);

    sol::state lua;
    lua.open_libraries();
    auto rana = lua["rana"].get_or_create<sol::table>();
    lua.safe_script(R"(
        function rana.error_handler(msg)
            local tb = debug.traceback(msg .. "\n", 2)
            rana._error = tb
            dlog(tb)
        end
    )", "=[rana setup]");
    lua.add_package_loader(fs_loader);
    sol::protected_function::set_default_handler(rana["error_handler"]);

    rana["fs"] = lua.require("fs", sol::c_call<decltype(&open_fs), &open_fs>, false);
    rana["log"] = lua.require("log", sol::c_call<decltype(&open_log), &open_log>, false);

    lua["require"]("main");

    sol::protected_function cb_rana_load        = lua["rana"]["load"];
    sol::protected_function cb_rana_draw        = lua["rana"]["draw"];
    sol::protected_function cb_rana_update      = lua["rana"]["update"];
    sol::protected_function cb_rana_configure   = lua["rana"]["configure"];

    auto cfg = lua.create_table();
    cb_rana_configure(cfg);

    auto ctx = rana::gfx::Context(
        cfg.get<const char *>("window_title"),
        cfg.get_or("window_width", 800),
        cfg.get_or("window_height", 600)
    );
    int canvas_width  = cfg.get_or("canvas_width", 0);
    int canvas_height = cfg.get_or("canvas_height", 0);
    if (canvas_width && canvas_height) {
        ctx.setCanvasSize(canvas_width, canvas_height);
    }

    auto font_oops = rana::gfx::Font::load("__rana/Vegur-Bold.otf", 72);
    auto font_traceback = rana::gfx::Font::load("__rana/Vegur-Regular.otf", 18);
    rana["_font_oops"] = &(*font_oops);
    rana["_font_default"] = &(*font_traceback);

    rana["gfx"] = &ctx;

    auto gfx_type = lua.new_usertype<rana::gfx::Context>("gfx_type",
        sol::constructors<rana::gfx::Context(const char *, int, int)>()
    );
    gfx_type["clear"] = [](rana::gfx::Context &self, float r, float g, float b) { 
        self.clear({r,g,b});
    };
    gfx_type["drawSprite"] = [](
        rana::gfx::Context &self, rana::gfx::Texture &tex,
        float x, float y, float w, float h, float rot,
        float r, float g, float b, float a
    ) {
        self.drawSprite(tex, {x,y}, {w,h}, rot, {r,g,b,a});
    };
    gfx_type["drawTextureSub"] = [](
        rana::gfx::Context &self, rana::gfx::Texture &tex,
        float x1, float y1, float x2, float y2,
        float x, float y,
        float r, float g, float b, float a 
    ) {
        self.drawTextureSub(tex, {x1,y1,x2,y2}, {x,y}, {r,g,b,a});
    };
    gfx_type["text"] = [](
        rana::gfx::Context &self, rana::gfx::Font &font, float x, float y, const char *text,
        float r, float g, float b, float a
    ) {
        self.text(font, text, glm::vec2{x,y}, {r,g,b,a});
    };
    gfx_type["loadTexture"] = &rana::gfx::Texture::load;
    gfx_type["resetTransform"] = &rana::gfx::Context::resetTransform;
    gfx_type["pushTransform"] =  &rana::gfx::Context::pushTransform;
    gfx_type["popTransform"] =   &rana::gfx::Context::popTransform;
    gfx_type["rotate"] = &rana::gfx::Context::rotate;
    gfx_type["translate"] = [](rana::gfx::Context &self, float x, float y) { self.translate({x,y}); };
    gfx_type["scale"] = [](rana::gfx::Context &self, float x, float y) { self.scale({x,y}); };
    gfx_type["loadFont"] = [](sol::this_state s, const char *fn, float size) {
        auto result = rana::gfx::Font::load(fn, size);
        if (result) {
            return std::move(*result);
        } else {
            luaL_error(s.lua_state(), result.error().c_str());
        }
    };
    gfx_type["fontSize"] = &rana::gfx::Context::fontSize;

    auto type_texture = lua.new_usertype<rana::gfx::Texture>("Texture");
    type_texture["setMinFilter"] = &rana::gfx::Texture::setMinFilter;

    cb_rana_load();

    enumerate_controllers();
    auto input = rana::input::Mapper();
    input.addMapping(Inputs::Left,  rana::input::JoyInput::LStickLeft,  SDL_SCANCODE_LEFT);
    input.addMapping(Inputs::Right, rana::input::JoyInput::LStickRight, SDL_SCANCODE_RIGHT);
    input.addMapping(Inputs::Jump,  rana::input::JoyInput::South,       SDL_SCANCODE_Z);
    std::vector<SDL_Gamepad *> enumerated_gamepads;
    enumerated_gamepads.reserve(16);
    SDL_Gamepad *current_gamepad = nullptr;

    bool running = true;
    auto ticks_ns = SDL_GetTicksNS();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type)
            {
            case SDL_EVENT_GAMEPAD_ADDED: {
                auto gde = (SDL_GamepadDeviceEvent *)&e;
                rana::log::info("gamepad connected: %s", SDL_GetGamepadNameForID(gde->which));
                auto gamepad = SDL_OpenGamepad(gde->which);
                if (gamepad) {
                    enumerated_gamepads.push_back(gamepad);
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_REMOVED: {
                auto gde = (SDL_GamepadDeviceEvent *)&e;
                if (SDL_GetGamepadFromID(gde->which) == current_gamepad) {
                    rana::log::info("lost current gamepad :(");
                    SDL_CloseGamepad(current_gamepad);
                    enumerated_gamepads = {}; // FIXME: why? shouldn't it just remove the removed one?
                    current_gamepad = nullptr;
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (!current_gamepad) {
                    auto gde = (SDL_GamepadDeviceEvent *)&e;
                    auto gamepad = SDL_GetGamepadFromID(gde->which);
                    if (!SDL_GamepadConnected(gamepad)) {
                        break;
                    }
                    rana::log::info("setting gamepad to current");
                    current_gamepad = gamepad;
                    for (auto n : enumerated_gamepads) {
                        if (n != current_gamepad) {
                            SDL_CloseGamepad(n);
                        }
                    }
                    enumerated_gamepads = {current_gamepad};
                }
                break;
            case SDL_EVENT_QUIT:
                running = false;
            }

            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        if (lua["rana"]["_error"] != sol::nil) {
            ctx.drawBegin();
            ctx.clear({0.95, 0.95, 0.95});

            ctx.fontSize(72);
            ctx.text(*font_oops, "oops.", {120, 156}, {0.73, 0.19, 0.39, 1});
            ctx.fontSize(18);
            ctx.text(*font_traceback, rana.get<const char*>("_error"), {120, 212}, {0.25, 0.25, 0.25, 1});

            ctx.drawFinish();
            synchronize_fps(1000.0/30.0);
            continue;
        }

        input.update(current_gamepad);
        auto ticks_new = SDL_GetTicksNS();
        cb_rana_update(double(ticks_new - ticks_ns) / 1000000000.0);
        ticks_ns = ticks_new;

        ctx.drawBegin();
        cb_rana_draw();
        ctx.drawFinish();

        synchronize_fps(1000.0 / 120.0);
        FrameMark;
    }

    rana::audio::deinit();

    return 0;
}

int main(int argc, char **argv)
{
    int result = -1;
    try {
        result = rana_main(argc, argv);
    } catch (std::exception &e) {
        auto msg = std::string("an exception has occured:\n\n") + e.what();
        rana::log::err(msg.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "error", msg.c_str(), nullptr);
    }
    return result;
}
