#pragma once

#include "common.hpp"
#include "samples.hpp"

namespace rana {
namespace audio {

struct PlaybackState {
    int id;
    DecodedSample *sample;
    bool is_playing;

    float pos = 0;
    float rate;
    float volume;
    float volume_target;
};

class Context {
    float sr;
    std::vector<PlaybackState> samples = std::vector<PlaybackState>(64);
    int last_sample_id = 0;
    int last_sample_index = 0;

public:
    Context(float sample_rate)
    {
        sr = sample_rate;
    }

    int playSample(DecodedSample *sample, float timestamp = -1)
    {
        auto &s = samples[last_sample_index];
        last_sample_index = (last_sample_index + 1) % samples.size();

        s.id = last_sample_id++;
        s.is_playing = true;
        s.pos = 0;
        s.sample = sample;
        s.volume = 1;
        s.volume_target = s.volume;
        s.rate = 1;

        return s.id;
    }

    void setVolume(int id, float volume)
    {
        // we can actually infer sample index based on last sample id
        int index = (last_sample_index - (last_sample_id - id)) % samples.size(); // fixme: is the math correct?
        auto &s = samples[index];
        //for (auto &s : samples) {
            if (s.id == id) {
                s.volume_target = s.volume = volume; // hack
            }
        //}
    }

    void process(SampleStereo *data, size_t n)
    {
        for (size_t i = 0; i < n; i++) {
            data[i].l = 0;
            data[i].r = 0;
        }

        for (auto &s : samples) {
            if (!s.is_playing) continue;

            auto size = s.sample->data.size();
            for (size_t i = 0; i < n; i++) {
                if (s.pos >= size) {
                    s.is_playing = false;
                    break;
                }

                data[i].l += s.sample->data[s.pos].l * s.volume;
                data[i].r += s.sample->data[s.pos].r * s.volume;
                s.pos += s.sample->rate / sr * s.rate;
            }
        }
    }
};

}
}