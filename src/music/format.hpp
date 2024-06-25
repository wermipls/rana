#pragma once

#include <stdint.h>
#include <vector>
#include "serializer.hpp"

namespace rana {
namespace musfmt {

enum class Interpolation : uint8_t {
    Linear,
    None,
    Hybrid,
};

enum class LoopMode : uint8_t {
    Off,
    OneShot,
    Forward,
    Backward,
    PingPong,
};

enum class Codec : uint8_t {
    FLAC,
    Opus,
};

// 0 = no sample,
// positive = module sample index,
// negative = global sample pool
typedef int32_t SampleDataID;

struct SampleData : public Serializable {
    Codec codec;
    std::vector<uint8_t> data;

    virtual void serialize(Serializer &s)
    {
        s.int8((uint8_t *)&codec);
        s.vector(data);
    }
};

struct Sample : public Serializable {
    float volume;
    float pan;
    int8_t transpose;
    int8_t fine;
    Interpolation interpolation;
    LoopMode loop_mode;
    uint32_t loop_start;
    uint32_t loop_end;
    SampleDataID sampledata_id;

    virtual void serialize(Serializer &s)
    {
        s.float32(&volume);
        s.float32(&pan);
        s.int8(&transpose);
        s.int8(&fine);
        s.int8((uint8_t *)&interpolation);
        s.int8((uint8_t *)&loop_mode);
        s.int32(&loop_start);
        s.int32(&loop_end);
        s.int32(&sampledata_id);
    }
};

struct ADSR : public Serializable {
    float attack  = 0.0;
    float hold    = 0.0;
    float decay   = 0.0;
    float sustain = 1.0;
    float release = 1.0;

    virtual void serialize(Serializer &s)
    {
        s.float32(&attack);
        s.float32(&hold);
        s.float32(&decay);
        s.float32(&sustain);
        s.float32(&release);
    }
};

struct Instrument : public Serializable {
    std::vector<Sample> smp;
    ADSR adsr_volume;

    virtual void serialize(Serializer &s)
    {
        uint32_t size = smp.size();
        s.int32(&size);
        smp.resize(size);
        for (auto &n : smp) {
            n.serialize(s);
        }

        adsr_volume.serialize(s);
    }
};

enum class EffectType : uint8_t {
    Lowpass,
    Highpass,
    Reverb,
    Delay,
    Distortion,
    Bitcrush,
    Compressor,
    Galactic,
};

struct Effect : public Serializable {
    EffectType type;
    std::vector<float> param;

    virtual void serialize(Serializer &s)
    {
        s.int8((uint8_t *)&type);

        uint8_t size = param.size();
        s.int8(&size);
        param.resize(size);
        for (auto &n : param) {
            s.float32(&n);
        }
    }
};

struct MixerTrack : public Serializable {
    std::string name;
    uint8_t columns;
    float volume;
    float pan;

    std::vector<Effect> fx;

    virtual void serialize(Serializer &s)
    {
        s.string8(name);
        s.int8(&columns);
        s.float32(&volume);
        s.float32(&pan);

        uint8_t size = fx.size();
        s.int8(&size);
        fx.resize(size);
        for (auto &n : fx) {
            n.serialize(s);
        }
    }
};

struct Mixer : public Serializable {
    std::vector<MixerTrack> tracks;
    std::vector<Effect> master_fx;
    float master_volume;

    virtual void serialize(Serializer &s)
    {
        s.float32(&master_volume);

        { // tracks
            uint8_t size = tracks.size();
            s.int8(&size);
            tracks.resize(size);
            for (auto &n : tracks) {
                n.serialize(s);
            }
        }

        { // master fx
            uint8_t size = master_fx.size();
            s.int8(&size);
            master_fx.resize(size);
            for (auto &n : master_fx) {
                n.serialize(s);
            }
        }
    }
};

enum class CommandType : uint8_t {
    Note,
    SleepLines,
    Instrument,
    Volume,
    Pan,
    FxArp,
    FxVibrato,
    FxFadeout,
    FxFadein,
    FxReverse,
    FxOffset,
    FxTempo,
    FxGlide,
    FxSlideUp,
    FxSlideDown,
    NoteLegato,
};

struct CommandParam {
    uint8_t y : 4;
    uint8_t x : 4;
};

struct Command {
    CommandType type;
    union {
        CommandParam param;
        uint8_t param_xy;
        uint8_t note;
    };

    void serialize(Serializer &s)
    {
        s.int8((uint8_t *)&type);
        s.int8(&param_xy);
    }
};

struct PatternChannel {
    std::vector<Command> rows;

    void serialize(Serializer &s)
    {
        uint16_t size = rows.size();
        s.int16(&size);
        rows.resize(size);
        for (auto &n : rows) {
            n.serialize(s);
        }
    }
};

struct Pattern : public Serializable {
    std::vector<PatternChannel> ch;
    uint16_t lines;

    virtual void serialize(Serializer &s)
    {
        uint8_t size = ch.size();
        s.int8(&size);
        ch.resize(size);
        for (auto &n : ch) {
            n.serialize(s);
        }

        s.int16(&lines);
    }
};

struct Song : public Serializable {
    float bpm;
    uint8_t beat_lines;
    uint8_t line_ticks;
    std::vector<Instrument> ins;
    Mixer mixer;
    std::vector<Pattern> patterns;
    std::vector<uint8_t> sequence;
    uint8_t loop_start;
    uint8_t loop_end;
    std::vector<SampleData> sampledata;

    virtual void serialize(Serializer &s)
    {
        s.float32(&bpm);
        s.int8(&beat_lines);
        s.int8(&line_ticks);

        { // instruments
            uint32_t size = ins.size();
            s.int32(&size);
            ins.resize(size);
            for (auto &n : ins) {
                n.serialize(s);
            }
        }

        mixer.serialize(s);

        { // patterns
            uint8_t size = patterns.size();
            s.int8(&size);
            patterns.resize(size);
            for (auto &n : patterns) {
                n.serialize(s);
            }
        }

        { // sequence
            uint8_t size = sequence.size();
            s.int8(&size);
            sequence.resize(size);
            for (auto &n : sequence) {
                s.int8(&n);
            }
        }

        { // samples
            uint16_t size = sampledata.size();
            s.int16(&size);
            sampledata.resize(size);
            for (auto &n : sampledata) {
                n.serialize(s);
            }
        }

        s.int8(&loop_start);
        s.int8(&loop_end);
    }
};

}
}
