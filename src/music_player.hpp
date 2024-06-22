#include "music_format.hpp"
#include "audio.hpp"
#include "audio_samples.hpp"
#include <cmath>
#include <memory>

namespace rana {
namespace audio {

double from_dB(double a)
{
    return std::pow(10, a/20);
}

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
    bool note_off = true;    // true -> stop generation after certain volume threshold
    bool note_triggered = 0; // true -> note has triggered this tick
    float pitch_actual = 0;
    float pitch_target = 0;
    float time_since_trigger = 0; // in seconds; used for resolving adsr etc.

    PlaybackSample sample{};
    int sample_pos = 0;
    float t = 0;
    SampleStereo prev{};

    musfmt::ADSR adsr;
    float adsr_multi = 1;

    float vibrato_phase = 0;
    float vibrato_speed = 0;
    float vibrato_intensity = 0;
    float vibrato_current = 0;

    float tick = 0;
    float line = 0;
    float arp_seq[3] = {1, 1, 1};
    float arp_current = 1;

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

        vibrato_current = 1;
        vibrato_intensity = 0;
        vibrato_phase = 0;
        vibrato_speed = 0;

        arp_seq[1] = 1;
        arp_seq[2] = 1;
        arp_current = 1;
        tick = 0;
    }

    void updateVolume()
    {
        volume_actual += (volume_target - volume_actual) * volume_coeff;
    }

    void updateVibrato(float deltatime)
    {
        vibrato_phase += vibrato_speed * M_PI * 2.0f * deltatime;
        vibrato_current = 1 + std::sin(vibrato_phase) * vibrato_intensity;
        //log::info("p %f in %f sp %f cur %f", vibrato_phase, vibrato_intensity, vibrato_speed, vibrato_current);
    }

    void sampleNext()
    {
        using enum musfmt::LoopMode;
        switch (sample.loop_mode) {
            case Off:
            case OneShot:
                if (sample_pos < sample.s->data.size()) sample_pos++;
                break;
            case Forward:
                sample_pos++;
                if (sample_pos == sample.loop_end) {
                    sample_pos = sample.loop_start;
                }
                break;
        }
    }

    void resetEffects()
    {
        setArpeggio(0,0);
        setVibrato(0,0);
    }

    void updateADSR()
    {
        float t = 0;
        t += adsr.attack;
        if (time_since_trigger < t) {
            adsr_multi = time_since_trigger / t;
            return;
        }
        t += adsr.hold;
        if (time_since_trigger < t) {
            return;
        }
        float u = t + adsr.decay;
        if (time_since_trigger < u) {
            adsr_multi = 1.0 - (time_since_trigger - t) / adsr.decay;
            adsr_multi = adsr.sustain + (adsr_multi * (1.0 - adsr.sustain));
            return;
        }

        adsr_multi = adsr.sustain;
    }

public:
    MusicSampler()
    {
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
            note_triggered = true;
        }
    }

    void setVolume(float volume)
    {
        if (note_triggered) {
            volume_actual = volume_target = volume;
        } else {
            volume_target = volume;
        }
    }

    void setVibrato(float speed, float intensity)
    {
        line = std::floor(line);
        vibrato_speed = speed;
        vibrato_intensity = intensity;
    }

    void setArpeggio(int x, int y)
    {
        line = std::floor(line);
        arp_seq[1] = intervalFromSemi(x);
        arp_seq[2] = intervalFromSemi(y);
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

        adsr = ins.adsr_volume;
    }

    bool isDisabled() {
        return note_off && volume_actual < 0.001; // -60dB threshold
    }

    inline static SampleStereo getSample(std::vector<SampleStereo> &s, size_t p)
    {
        if (p >= s.size()) return SampleStereo{0,0};
        return s[p];
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        note_triggered = false;
        using enum musfmt::Interpolation;
        auto buf = std::vector<SampleStereo>(n_samples);

        if (isDisabled()) {
            return buf;
        }

        updateVibrato((float)n_samples / sr);

        auto &smpdat = sample.s->data;
        SampleStereo smp;

        for (auto &a : buf) {
            updateVolume();
            doSeconds(1.0f / sr);
            updateADSR();
            smp = getSample(smpdat, sample_pos);
            float tt = t;
            if (sample.interpolation == None) {
                tt = 0;
            }
            a.l = smp.l * tt + prev.l * (1.f - tt);
            a.r = smp.r * tt + prev.r * (1.f - tt);
            a.l *= volume_actual * adsr_multi * sample.volume;//a.l *= volume * pan_factors.l;
            a.r *= volume_actual * adsr_multi * sample.volume;//a.r *= volume * pan_factors.r;
            if (sample.interpolation != Linear) {
                prev = smp;
            }

            t += pitch_actual * vibrato_current * arp_current * sample.transpose_fine / 261.6255f; //FIXME
            while (t > 1.f) {
                t -= 1.f;
                if (sample.interpolation == Linear) {
                    prev = getSample(smpdat, sample_pos);
                }
                sampleNext();
            }
        }

        return buf;
    }

    void doTicks(float ticks)
    {
        tick += ticks;
        arp_current = arp_seq[int(tick/2) % 3]; // FIXME: what is this about?
    }

    void doLines(float lines)
    {
        line += lines;
        if (line >= 1) {
            line = 0;
            resetEffects();
        }
    }

    void doSeconds(float seconds)
    {
        updateADSR();
        time_since_trigger += seconds;
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

    void getSamples(std::vector<SampleStereo> &out, float volume)
    {
        auto size = out.size();

        for (size_t i = 0; i < voices; i++) {
            auto sampler_out = samplers[i].getSamples(size);

            for (size_t j = 0; j < size; j++) {
                out[j].l += sampler_out[j].l * volume;
                out[j].r += sampler_out[j].r * volume;
            }
        }
    }
};

class Track {
    float sr;
    std::vector<std::unique_ptr<Effect>> fx;

public:
    Track(std::vector<musfmt::Effect> &fx, float sample_rate) : sr{sample_rate}
    {
        for (auto &n : fx) {
            addEffect(n);
        }
    }

    void addEffect(musfmt::Effect &effect)
    {
        Effect *instance = nullptr;
        using enum musfmt::EffectType;
        switch (effect.type) {
            case Lowpass:    instance = new audio::Filter1Pole(sr, false); break;
            case Highpass:   instance = new audio::Filter1Pole(sr, true); break;
            case Reverb:     instance = new audio::Reverb(); break;
            case Delay:      instance = new audio::Delay(sr); break;
            case Distortion: instance = new audio::Distortion(); break;
            case Bitcrush:   instance = new audio::Bitcrush(); break;
            case Galactic:   instance = new audio::Galactic(); break;
        }

        if (instance == nullptr) {
            return;
        }

        for (size_t i = 0; i < effect.param.size(); i++) {
            instance->setParam(i, effect.param[i]);
        }

        fx.push_back(std::unique_ptr<Effect>(instance));
    }

    void process(SampleStereo *in, size_t n_samples)
    {
        for (auto &n : fx) {
            n->process(in, n_samples);
        }
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

    size_t ch_count = 0;
    std::vector<int> cmd_i;
    std::vector<float> sleep_lines;
    std::vector<Channel> channel;
    std::vector<std::shared_ptr<DecodedSample>> decoded_sample;

    int sequence_pos = 0;

    std::vector<uint8_t> ch_to_track;
    std::vector<Track> tracks;

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
        lines_left = currentPattern().lines;
        recalculateSamplesTick();

        for (auto &n : song.sampledata) {
            auto d = decode_flac(n.data);
            if (d == nullptr) {
                log::err("failed to decode sample...");
                abort();
            }
            decoded_sample.push_back(d);
        }

        for (auto &n : song.mixer.tracks) {
            ch_count += n.columns;
        }

        cmd_i.resize(ch_count);
        sleep_lines.resize(ch_count);
        channel.resize(ch_count);
        for (auto &n : channel) {
            auto sampler = n.sampler();
            auto sample_id = song.ins[0].smp[0].sampledata_id;
            n.setInstrument(song.ins[0], decoded_sample[sample_id].get());
        }

        for (int i = 0; i < song.mixer.tracks.size(); i++) {
            auto &track = song.mixer.tracks[i];
            tracks.push_back(Track(track.fx, sr));
            for (int j = 0; j < track.columns; j++) {
                ch_to_track.push_back(i);
            } 
        }
    }

    void nextPattern()
    {
        sequence_pos++;
        if (sequence_pos > song.loop_end) {
            sequence_pos = song.loop_start;
        }
    }

    const musfmt::Pattern &currentPattern()
    {
        return song.patterns[song.sequence[sequence_pos]];
    }

    void doCommand(int column, musfmt::Command cmd)
    {
        auto sampler = channel[column].sampler();
        using enum musfmt::CommandType;
        switch (cmd.type) {
            case Note: {
                channel[column].note(cmd.note);
                break;
            }
            case SleepLines:
                sleep_lines[column] += cmd.param_xy;
                break;
            case Volume:
                sampler->setVolume((float)cmd.param_xy / 80.0);
                break;
            case Instrument: {
                auto &ins = song.ins[cmd.param_xy];
                auto sid = ins.smp[0].sampledata_id;
                channel[column].setInstrument(ins, decoded_sample[sid].get());
                break;
            }
            case FxArp:
                sampler->setArpeggio(cmd.param.x, cmd.param.y);
                break;
            case FxVibrato:
                sampler->setVibrato(
                    (float)cmd.param.x/15.0f * sr / (samples_tick * (float)ticks_line),
                    (std::pow(2.0f, 2.0f/12.0f) - 1.0f) * (float)cmd.param.y/15.0f
                );
                break;
            case FxTempo:
                setBPM(cmd.param_xy);
                break;
        }
    }

    void doSequence(size_t samples)
    {
        if (lines_left <= 0) {
            nextPattern();
            lines_left = currentPattern().lines;
            for (size_t i = 0; i < ch_count; i++) {
                cmd_i[i] = 0;
                sleep_lines[i] = 0;
            }
        }
        for (size_t i = 0; i < ch_count; i++) {
            while (sleep_lines[i] <= 0) {
                auto &rows = currentPattern().ch[i].rows;
                if (cmd_i[i] < rows.size()) {
                    auto cmd = rows[cmd_i[i]];
                    doCommand(i, cmd);
                    cmd_i[i]++;
                } else {
                    break;
                }
            }
            channel[i].sampler()->doTicks(samples / samples_tick);
            sleep_lines[i] -= samples / samples_tick / (float)ticks_line;
            channel[i].sampler()->doLines(samples / samples_tick / (float)ticks_line);
        }

        lines_left -= samples / samples_tick / (float)ticks_line;
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        doSequence(n_samples);

        std::vector<SampleStereo> samples(n_samples);

        for (size_t i = 0; i < tracks.size(); i++) {
            std::vector<SampleStereo> track_buf(n_samples);

            for (size_t j = 0; j < ch_to_track.size(); j++) {
                if (ch_to_track[j] == i) {
                    channel[j].getSamples(track_buf, from_dB(-6));
                }
            }

            tracks[i].process(track_buf.data(), track_buf.size());
            auto volume = song.mixer.tracks[i].volume;

            for (size_t j = 0; j < samples.size(); j++) {
                samples[j].l += track_buf[j].l * volume;
                samples[j].r += track_buf[j].r * volume;
            }
        }

        return samples;
    }

    void setTicks(int ticks) { ticks_line = ticks; recalculateSamplesTick(); }
    void setLines(int lines) { lines_beat = lines; recalculateSamplesTick(); }
    void setBPM(int bpm) { this->bpm = bpm; recalculateSamplesTick(); log::info("bpm: %f", this->bpm); }
};

}
}
