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
    double volume;
    double pan;
    int8_t transpose;
    int8_t fine;
    Interpolation interpolation;
    LoopMode loop_mode;
    uint32_t loop_start;
    uint32_t loop_end;
    SampleDataID sampledata_id;

    virtual void serialize(Serializer &s)
    {
        s.float64(&volume);
        s.float64(&pan);
        s.int8(&transpose);
        s.int8(&fine);
        s.int8((uint8_t *)&interpolation);
        s.int8((uint8_t *)&loop_mode);
        s.int32(&loop_start);
        s.int32(&loop_end);
        s.int32(&sampledata_id);
    }
};

struct Instrument : public Serializable {
    std::vector<Sample> smp;

    virtual void serialize(Serializer &s)
    {
        uint32_t size = smp.size();
        s.int32(&size);
        smp.resize(size);
        for (auto &n : smp) {
            n.serialize(s);
        }
    }
};

struct MixerTrack : public Serializable {
    std::string name;
    double volume;
    double pan;

    virtual void serialize(Serializer &s)
    {
        s.string8(name);
        s.float64(&volume);
        s.float64(&pan);
    }
};

struct Mixer : public Serializable {
    std::vector<MixerTrack> tracks;
    double master_volume;

    virtual void serialize(Serializer &s)
    {
        uint32_t size = tracks.size();
        s.int32(&size);
        tracks.resize(size);
        for (auto &n : tracks) {
            n.serialize(s);
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
};

struct Command {
    CommandType type;
    union {
        struct {
            uint8_t x : 4;
            uint8_t y : 4;
        } param;
        uint8_t param_xy;
        uint8_t note;
    };

    void serialize(Serializer &s)
    {
        s.int8((uint8_t *)&type);
        s.int8(&param_xy);
    }
};

struct PatternCol {
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

struct PatternTrack {
    std::vector<PatternCol> col;

    void serialize(Serializer &s)
    {
        uint8_t size = col.size();
        s.int8(&size);
        col.resize(size);
        for (auto &n : col) {
            n.serialize(s);
        }
    }
};
struct Pattern : public Serializable {
    std::vector<PatternTrack> tracks;

    virtual void serialize(Serializer &s)
    {
        uint8_t size = tracks.size();
        s.int8(&size);
        tracks.resize(size);
        for (auto &n : tracks) {
            n.serialize(s);
        }
    }
};

struct Song : public Serializable {
    double bpm;
    uint8_t beat_lines;
    uint8_t line_ticks;
    std::vector<Instrument> ins;
    Mixer mixer;
    std::vector<Pattern> patterns;
    std::vector<uint8_t> sequence;
    uint8_t loop_start;
    uint8_t loop_end;

    virtual void serialize(Serializer &s)
    {
        s.float64(&bpm);
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

        s.int8(&loop_start);
        s.int8(&loop_end);
    }
};

}
}
