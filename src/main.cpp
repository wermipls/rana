#include <SDL2/SDL.h>
#include "gfx.hpp"
#include "audio/audio.hpp"
#include "music/player.hpp"
#include "serializer.hpp"
#include "log.hpp"
#include "fio.hpp"

int main(int argc, char **argv)
{
    if (argc == 0) {
        return -1;
    }

    auto ctx = rana::gfx::Context();
    rana::audio::init(44100);

    std::vector<uint8_t> mus;
    if (!rana::readfile(mus, "out.ranamus")) {
        rana::log::err("failed to read music file...");
        return -1;
    };
    auto song = rana::musfmt::Song();
    auto songser = rana::Serializer(mus);
    song.serialize(songser);

    auto player = rana::audio::MusicPlayer(song, 44100);

    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type)
            {
            case SDL_QUIT:
                return 0;
            }
        }

        if (rana::audio::needs_more_data()) {
            rana::audio::queue(player.getSamples(32));
        } else {
            SDL_Delay(1);
            continue;
        }
    }

    return 0;
}
