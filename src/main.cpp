#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include "gfx.hpp"
#include "gfx_textures.hpp"
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

float axis(float p1, float s1, float p2, float s2)
{
    float delta;

    if (p1 < p2) {
        delta = (p1 + s1) - (p2 - s2);
        if (delta < 0.f) {
            return 0;
        }
    } else {
        delta = (p1 - s1) - (p2 + s2);
        if (delta > 0.f) {
            return 0;
        }
    }
    return delta;
}

glm::vec2 aabb(glm::vec2 p1, glm::vec2 s1, glm::vec2 p2, glm::vec2 s2)
{
    s1 *= 0.5f;
    s2 *= 0.5f;
    glm::vec2 v{0.f};
    glm::vec2 delta;
    delta.x = axis(p1.x, s1.x, p2.x, s2.x);
    if (!delta.x) {
        return v;
    }
    delta.y = axis(p1.y, s1.y, p2.y, s2.y);
    if (!delta.y) {
        return v;
    }

    if (auto ad = glm::abs(delta); ad.x >= ad.y) {
        delta.x = 0;
    } else {
        delta.y = 0;
    }

    return delta;
}

struct Player {
    glm::vec2 pos{};
    glm::vec2 size{};

    glm::vec2 speed{};

    float gravity = 0.1f;
    bool grounded = false;

    void doTick()
    {
        auto kbstate = SDL_GetKeyboardState(nullptr);
        if (kbstate[SDL_SCANCODE_RIGHT]) {
            speed.x = std::min(speed.x += 0.4f, 3.5f);
        } else if (kbstate[SDL_SCANCODE_LEFT]) {
            speed.x = std::max(speed.x -= 0.4f, -3.5f);
        } else {
            speed.x = speed.x * (grounded ? 0.2f : 0.99f);
        }

        if (grounded && kbstate[SDL_SCANCODE_Z]) {
            speed.y = -6.f;
        }
        grounded = false;
        speed.y += gravity;
        pos += speed;
    }

    void test(glm::vec2 bpos, glm::vec2 bsize)
    {
        auto v = aabb(bpos, bsize, pos, size);
        pos += v;

        if (v.y < 0.f && speed.y > 0.0f) {
            speed.y = 0;
            grounded = true;
        }

        if (v.y > 0.f && speed.y < 0.0f) {
            speed.y = speed.y * -0.2f;
        }

        if (v.x > 0.f && speed.x < 0.0f) {
            speed.x = 0;
        }

        if (v.x < 0.f && speed.x > 0.0f) {
            speed.x = 0;
        }
    }
};

int main(int argc, char **argv)
{
    if (argc == 0) {
        return -1;
    }

    rana::fs::init(argv[0]);

    auto ctx = rana::gfx::Context();

    std::vector<uint8_t> mus;
    if (!rana::fs::readfile(mus, "out.ranamus")) {
        return -1;
    };
    auto song = rana::musfmt::Song();
    auto songser = rana::Serializer(mus);
    song.serialize(songser);

    auto player = rana::audio::MusicPlayer(song, 44100);
    rana::audio::init(44100, audio_callback, &player);

    auto brick = rana::gfx::load_texture("brik.png");
    auto chara = rana::gfx::load_texture("chara.png");

    uint8_t map[24*16] = {
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0,
        0,0,1,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

        0,0,1,0, 0,0,0,0, 0,1,1,1, 1,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 1,1,1,1, 1,1,1,1, 0,0,0,0, 0,0,0,0, 0,0,0,0,

        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    };

    auto plr = Player();
    plr.size = {64, 64};
    plr.pos = {440, 100};

    bool running = true;
    float time = 0;
    int ticks_done = SDL_GetTicks() / 1000.0f * 120;
    int ticks_target;

    while (running) {
        time = SDL_GetTicks() / 1000.0f;
        ticks_target = time * 120;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
            }

            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        ctx.drawBegin();
        ctx.clear({0.7, 0.5, 0.6});

        float x, y;
        SDL_GetMouseState(&x, &y);

        if (ticks_done < ticks_target) {
            ticks_done++;
            plr.doTick();
        }
        for (int x = 0; x < 24; x++) {
            for (int y = 0; y < 16; y++) {
                if (!map[y * 24 + x]) continue;
                glm::vec2 pos{x * 48, y * 48};
                glm::vec2 size{48,48};
                ctx.drawSprite(brick, pos-size/2.0f, size, 0);
                plr.test(pos, size);
            }
        }
        ctx.drawSprite(chara, plr.pos-plr.size/2.0f, plr.size, 0, 1.0f);

        //ImGui::ShowDemoWindow();
        //player.drawMixer();
        //player.drawPattern();

        ctx.drawFinish();
        FrameMark;
    }

    rana::audio::deinit();

    return 0;
}
