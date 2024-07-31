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

    auto tex = rana::gfx::load_texture("punch.png");

    bool running = true;
    float time = 0;
    while (running) {
        time = SDL_GetTicks() / 3000.0f;
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

        float x, y;
        SDL_GetMouseState(&x, &y);

        {
            ZoneScopedN("Particles");
            for (int i = 0; i < 512; i++) {
                float a = i / 512.f;
                auto b = a + 1.0f;
                float size = (sin(time/10.f + a) + 0.1f) * 64.f;
                ctx.drawSprite(tex, {640+cos(time*b*(time/7.0f+1.0f))*300.f+(a-.5f)*100.f,360+sin(time*3.0f*b)*200.0f+(a-.5f)*100.f}, {size,size}, 0, a * 0.1f);
            }
        }
        ctx.drawSprite(tex, {x,y}, {256,256}, 0);
        

        ImGui::ShowDemoWindow();
        player.drawMixer();
        player.drawPattern();

        ctx.drawFinish();
        FrameMark;
    }

    rana::audio::deinit();

    return 0;
}
