#include <SDL3/SDL_main.h>
#include <sdl3/SDL.h>
#include "gfx.hpp"
#include "audio/backend.hpp"
#include "music/player.hpp"
#include "serializer.hpp"
#include "log.hpp"
#include "fs.hpp"

void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    auto player = (rana::audio::MusicPlayer *)userdata;
    while (additional_amount > 0) {
        auto samples = player->getSamples(1);
        auto bytes = samples.size() * 8;
        SDL_PutAudioStreamData(stream, samples.data(), bytes);
        additional_amount -= bytes;
    }
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

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
            }
        }

        SDL_Delay(1);
    }

    rana::audio::deinit();

    return 0;
}
