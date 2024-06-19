#include "music_format.hpp"
#include "audio.hpp"
#include "audio_samples.hpp"
#include <cmath>
#include <memory>

namespace rana {
namespace audio {

double intervalFromSemi(double semitones)
{
    return std::pow(2, (double)semitones / 12.0);
}

Hz freqFromNote(uint8_t note)
{
    constexpr auto A4 = 4*12 + 10;
    constexpr auto tuning_A4 = 440.0;

    int interval = note - A4;

    return tuning_A4 * intervalFromSemi(interval);
}

struct PlaybackSample {
    DecodedSample *s;
    float volume;
    float pan;
    float transpose_fine;
    musfmt::Interpolation interpolation;
    musfmt::LoopMode loop_mode;
    uint32_t loop_start;
    uint32_t loop_end;
};


class MusicSampler {
    const float sr = 44100;

    float volume_target = 0;
    float volume_actual = 0;
    float volume_coeff = 0;
    bool note_off = 0;       // true -> stop generation after certain volume threshold
    bool note_triggered = 0; // true -> note has triggered this tick
    float pitch_actual = 0;
    float pitch_target = 0;
    float time_since_trigger = 0; // in seconds; used for resolving adsr etc.

    PlaybackSample sample{};
    int sample_pos = 0;
    float t = 0;
    SampleStereo prev{};

    void resetState()
    {
        volume_actual = 1;
        volume_target = 1;
        volume_coeff = factor_1pole(RnsVolumeSmoothing, sr);
        note_off = false;
        time_since_trigger = 0;

        sample_pos = 0;
        t = 0;
        prev = {0,0};
    }

    void updateVolume()
    {
        volume_actual += (volume_target - volume_actual) * volume_coeff; 
    }

    void sampleNext()
    {
        using enum musfmt::LoopMode;
        switch (sample.loop_mode) {
            case Off:
            case OneShot:
                if (sample_pos < sample.s->data.size() - 1) sample_pos++;
                break;
            case Forward:
                sample_pos++;
                if (sample_pos == sample.loop_end) {
                    sample_pos = sample.loop_start;
                }
                break;
        }
    }

public:
    MusicSampler()
    {
        resetState();
    }

    void noteOff()
    {
        if (note_off) return;

        volume_target = 0;
        volume_coeff = factor_1pole(RnsDeclickSmoothing, sr);
    }

    void setNote(uint8_t note)
    {
        if (note) {
            resetState();
            pitch_actual = pitch_target = freqFromNote(note);
        }
    }

    void setVolume(float volume)
    {
        if (note_triggered) {
            volume_actual = volume_target = volume;
        } else {
            volume_target = volume_actual;
        }
    }

    void setInstrument(const musfmt::Instrument &ins, DecodedSample *smp)
    {
        sample.s = smp;
        auto &s = ins.smp[0];
        sample.volume        = s.volume;
        sample.pan           = s.pan;
        sample.loop_mode     = s.loop_mode;
        sample.loop_start    = s.loop_start;
        sample.loop_end      = s.loop_end;
        sample.interpolation = s.interpolation;

        sample.transpose_fine = intervalFromSemi((double)s.transpose + (double)s.fine / 127);
        sample.transpose_fine *= (smp->rate / sr);
    }

    bool isDisabled() {
        return note_off && volume_actual < 0.001; // -60dB threshold
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        using enum musfmt::Interpolation;
        auto buf = std::vector<SampleStereo>(n_samples);

        //if (isDisabled()) {
        //    return buf;
        //}

        auto &smpdat = sample.s->data;
        SampleStereo smp;

        for (auto &a : buf) {
            updateVolume();
            smp = smpdat[sample_pos];
            float tt = t;
            if (sample.interpolation == None) {
                tt = 0;
            }
            a.l = smp.l * tt + prev.l * (1.f - tt);
            a.r = smp.r * tt + prev.r * (1.f - tt);
            a.l *= volume_actual * sample.volume;//a.l *= volume * pan_factors.l;
            a.r *= volume_actual * sample.volume;//a.r *= volume * pan_factors.r;
            if (sample.interpolation != Linear) {
                prev = smp;
            }

            t += pitch_actual * sample.transpose_fine / 261.6255f; //FIXME
            while (t > 1.f) {
                t -= 1.f;
                if (sample.interpolation == Linear) {
                    prev = smpdat[sample_pos];
                }
                sampleNext();
            }
        }

        return buf;
    }
};

class Channel {
    static constexpr auto voices = 2;

    std::vector<MusicSampler> samplers;
    size_t current = 0;

public:
    Channel()
    {
        samplers.resize(voices);
    }

    MusicSampler *sampler()
    {
        return &samplers[current];
    }

    void note(uint8_t note)
    {
        samplers[current].noteOff();
        if (note) {
            current = (current + 1) % voices;
            samplers[current].setNote(note);
        }
    }

    void setInstrument(const musfmt::Instrument &ins, DecodedSample *smp)
    {
        for (auto &n : samplers) {
            n.setInstrument(ins, smp);
        }
    }

    void getSamples(std::vector<SampleStereo> &out)
    {
        auto size = out.size();
        std::vector<SampleStereo> buf(size);

        for (size_t i = 0; i < voices; i++) {
            auto sampler_out = samplers[i].getSamples(size);

            for (size_t j = 0; j < size; j++) {
                buf[j].l += sampler_out[j].l;
                buf[j].r += sampler_out[j].r;
            }
        }

        out = buf;
    }
};

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
    Channel channel;
    std::unique_ptr<DecodedSample> decoded_sample;

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
        decoded_sample = decode_flac(sample_data);
        if (decoded_sample == nullptr) {
            abort();
        }

        auto sampler = channel.sampler();
        channel.setInstrument(song.ins[0], decoded_sample.get());
    }

    void doCommand(int column, musfmt::Command cmd)
    {
        auto sampler = channel.sampler();
        using enum musfmt::CommandType;
        switch (cmd.type) {
            case Note: {
                channel.note(cmd.note);
                break;
            }
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

        std::vector<SampleStereo> samples(n_samples); 

        channel.getSamples(samples);

        for (auto &n : samples) {
            n.l *= 0.5;
            n.r *= 0.5;
        }

        return samples;
    }

    void setTicks(int ticks) { ticks_line = ticks; recalculateSamplesTick(); }
    void setLines(int lines) { lines_beat = lines; recalculateSamplesTick(); }
    void setBPM(int bpm) { this->bpm = bpm; recalculateSamplesTick(); }
};

}
}
