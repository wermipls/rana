#include <SDL2/SDL.h>
#include "gfx.hpp"
#include "audio.hpp"
#include "serializer.hpp"
#include <fmt/core.h>

int main(int argc, char **argv)
{
    if (argc == 0) {
        return -1;
    }

    auto ctx = rana::gfx::Context();
    auto osc = rana::audio::Sine(44100, 440, 0.0, 4.0);
    rana::audio::init(44100);

    auto sample = rana::audio::load_sample("C:\\Users\\lea\\Music\\loop_repro_test.wav");
    if (sample == nullptr) {
        return -1;
    }

    sample->setLooping(true);
    auto sampler = rana::audio::Sampler(
        sample,
        440,
        0.05,
        true,
        rana::audio::None
    );

    auto track = rana::audio::Track();
    auto sources = std::vector<rana::audio::Saw>();

    auto lp = rana::audio::Lowpass(44100, 0);

    std::vector<int> notes = {0, 3, 7, 12, 12+3, 12+7, 24, 24+3, 24+7};

    for (auto &n : notes) {
        sources.push_back(rana::audio::Saw(44100, 100 * std::pow(2, n/12.f), 0.05));
    }

    for (auto &n : sources) {
        track.addSource(&n);
    }

    uint64_t ticks = 0;

    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type)
            {
            case SDL_QUIT:
                return 0;
            case SDL_MOUSEBUTTONDOWN: {
                int x, y;
                int bp = SDL_GetMouseState(&x, &y);
                if (bp & SDL_BUTTON(1)) {
                    sample->seek(x / 1280.f);
                    sample->setReverse(false);
                } else if (bp & SDL_BUTTON(3)) {
                    sample->setReverse(true);
                }
            }
            }
        }

        int y;
        if (SDL_GetMouseState(0, &y) & SDL_BUTTON(2)) {
            lp.setCutoff(20000.f * (float)y / 720.f);
        }

        if (rana::audio::needs_more_data()) {
            //rana::audio::queue(osc.getSamples(256));
            rana::audio::queue(lp.process(sampler.getSamples(256)));
            //rana::audio::queue(track.getSamples(256));
            ticks+=10;
        } else {
            SDL_Delay(1);
            continue;
        }
        if ((ticks % 1000) > 500) {
            osc.setVolume(0.1);
        } else {
            osc.setVolume(0.7);
        }

        for (size_t i = 0; i < sources.size(); i++) {
            sources[i].setVolume((cos((float)ticks / ((float)(i*7+19)*10.f)) + 1.f) * 0.05f);
        }

        sampler.setFrequency(440+std::cos(ticks / 2000.f) * 50.f);

        osc.setFrequency(300+std::cos(ticks / 2000.f) * 250.f);
        osc.setPan(std::cos(ticks / 4000.f));
    }

    return 0;
}
