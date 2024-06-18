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

    int cur_line = 0;
    int cur_tick = 0;
    float t = 0;

    int cmd_i = 0;
    float sleep_ticks = 0;
    Saw saw;

    void recalculateSamplesTick()
    {
        samples_tick = sr / (bpm / 60.0 * float(lines_beat * ticks_line));
    }

public:
    MusicPlayer(musfmt::Song song, float sr = 44100) : song{song}, sr{sr}, saw{Saw(sr, 0, 0.5)}
    {
        bpm = song.bpm;
        ticks_line = song.line_ticks;
        lines_beat = song.beat_lines;
        recalculateSamplesTick();
    }

    void doCommand(int column, musfmt::Command cmd)
    {
        using enum musfmt::CommandType;
        switch (cmd.type) {
            case Note:
                if (cmd.note) {
                    saw.setVolume(0.2);
                    saw.setFrequency(freqFromNote(cmd.note));
                } else {
                    saw.setVolume(0);
                }
                break;
            case SleepLines:
                sleep_ticks += ticks_line * cmd.param_xy;
                break;
        }
    }

    void doSequence(size_t samples)
    {
        while (sleep_ticks <= 0) {
            auto &rows = song.patterns[0].tracks[0].col[0].rows;
            if (cmd_i < rows.size()) {
                auto cmd = rows[cmd_i];
                doCommand(0, cmd);
                cmd_i++;
            } else {
                break;
            }
        }

        sleep_ticks -= samples / samples_tick;
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        doSequence(n_samples);
        
        return saw.getSamples(n_samples);
    }

    void setTicks(int ticks) { ticks_line = ticks; recalculateSamplesTick(); }
    void setLines(int lines) { lines_beat = lines; recalculateSamplesTick(); }
    void setBPM(int bpm) { this->bpm = bpm; recalculateSamplesTick(); }
};

}
}
