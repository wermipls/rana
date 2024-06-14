#pragma once

#include <stdint.h>
#include <vector>
#include "serializer.hpp"

namespace rana {
namespace musfmt {

struct Sample : public Serializable {
    double volume;
    double pan;
    int8_t transpose;
    int8_t fine;

    virtual void serialize(Serializer &s)
    {
        s.float64(&volume);
        s.float64(&pan);
        s.int8(&transpose);
        s.int8(&fine);
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
