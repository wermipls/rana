#include "music_format.hpp"
#include "audio.hpp"
#include "log.hpp"
#include <cmath>

namespace rana {
namespace audio {

Hz freqFromNote(uint8_t note)
{
    constexpr auto A4 = 4*12 + 10;
    constexpr auto tuning_A4 = 440.0;

    int interval = note - A4;

    return tuning_A4 * std::pow(2, (double)interval / 12.0);
}

class MusicPlayer {
    const musfmt::Song song;
    const float sr;
    float samples_tick;
    float bpm;
    int ticks_line;
    int lines_beat;

    float lines_left = 0;

    int cmd_i = 0;
    float sleep_lines = 0;
    Sampler *sampler = nullptr;

    void recalculateSamplesTick()
    {
        samples_tick = sr / (bpm / 60.0 * float(lines_beat * ticks_line));
    }

public:
    MusicPlayer(musfmt::Song song, float sr = 44100) : song{song}, sr{sr}
    {
        bpm = song.bpm;
        ticks_line = song.line_ticks;
        lines_beat = song.beat_lines;
        lines_left = song.patterns[0].lines;
        recalculateSamplesTick();

        auto sample_id = song.ins[0].smp[0].sampledata_id;
        auto &sample_data = song.sampledata[sample_id].data;
        auto sample = load_flac(sample_data.data(), sample_data.size());
        if (sample == nullptr) {
            abort();
        }
        if (song.ins[0].smp[0].loop_mode == musfmt::LoopMode::Forward) {
            sample->setLooping(true);
        }

        sample->loop_start = song.ins[0].smp[0].loop_start;
        sample->loop_end = song.ins[0].smp[0].loop_end;
        sampler = new Sampler(sample);
    }

    ~MusicPlayer()
    {
        delete sampler;
    }

    void doCommand(int column, musfmt::Command cmd)
    {
        using enum musfmt::CommandType;
        switch (cmd.type) {
            case Note:
                if (cmd.note) {
                    sampler->setVolume(0.2);
                    sampler->setFrequency(freqFromNote(cmd.note + song.ins[0].smp[0].transpose));
                } else {
                    sampler->setVolume(0);
                }
                break;
            case SleepLines:
                sleep_lines += cmd.param_xy;
                break;
        }
    }

    void doSequence(size_t samples)
    {
        if (lines_left <= 0) {
            lines_left += song.patterns[0].lines;
            cmd_i = 0;
            sleep_lines = 0;
        }
        while (sleep_lines <= 0) {
            auto &rows = song.patterns[0].tracks[0].col[0].rows;
            if (cmd_i < rows.size()) {
                auto cmd = rows[cmd_i];
                doCommand(0, cmd);
                cmd_i++;
            } else {
                break;
            }
        }

        sleep_lines -= samples / samples_tick / (float)ticks_line;
        lines_left -= samples / samples_tick / (float)ticks_line;
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        doSequence(n_samples);

        auto samples = sampler->getSamples(n_samples);

        for (auto &n : samples) {
            n.l * 0.2;
            n.r * 0.2;
        }

        return samples;
    }

    void setTicks(int ticks) { ticks_line = ticks; recalculateSamplesTick(); }
    void setLines(int lines) { lines_beat = lines; recalculateSamplesTick(); }
    void setBPM(int bpm) { this->bpm = bpm; recalculateSamplesTick(); }
};

}
}
