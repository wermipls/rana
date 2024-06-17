#pragma once

#include <stdint.h>
#include <vector>
#include "serializer.hpp"

namespace rana {
namespace musfmt {

enum Interpolation : uint8_t {
    Linear,
    None,
    Hybrid,
};

enum LoopMode : uint8_t {
    Off,
    OneShot,
    Forward,
    Backward,
    PingPong,
};

enum Codec : uint8_t {
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

struct Song : public Serializable {
    double bpm;
    uint8_t beat_lines;
    uint8_t line_ticks;
    std::vector<Instrument> ins;

    virtual void serialize(Serializer &s)
    {
        s.float64(&bpm);
        s.int8(&beat_lines);
        s.int8(&line_ticks);

        uint32_t size = ins.size();
        s.int32(&size);
        ins.resize(size);
        for (auto &n : ins) {
            n.serialize(s);
        }
    }
};

}
}
