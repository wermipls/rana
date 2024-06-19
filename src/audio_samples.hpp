#pragma once
#include <vector>
#include <cmath>
#include <stdint.h>
#include <stddef.h>
#include <memory>

#include "audio_common.hpp"

namespace rana {
namespace audio {

class Sample {
protected:
    bool looped = false;
    bool reverse = false;
    size_t position = 0;

public:
    size_t loop_start = 0;
    size_t loop_end = 0;
    virtual std::vector<SampleStereo> getSamples(size_t n_samples) = 0;
    virtual void seek(float pos) = 0;
    virtual void setLooping(bool is_looping) = 0;
    virtual void setReverse(bool reversed) = 0;
    virtual double getSampleRate() = 0;
};

class SampleMonoS16 : public Sample {
    std::shared_ptr<std::vector<int16_t>> data;
    Hz sr;

public:
    SampleMonoS16(std::shared_ptr<std::vector<int16_t>> data, Hz sample_rate)
    {
        this->data = data;
        loop_end = data->size() - 1;
        sr = sample_rate;
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        std::vector<SampleStereo> buf(n_samples);
        int16_t *d = data->data(); 
        auto size = data->size();

        for (auto &s : buf) {
            if (position >= size) {
                continue;
            }
            s.l = s.r = d[position] / (float)INT16_MAX;

            if (!reverse) {
                position++;
                if (looped && position >= loop_end) {
                    position = loop_start;
                }
            } else {
                position--;
                if (looped && position <= loop_start) {
                    position = loop_end;
                }
            }
        }

        return buf;
    }

    void seek(float pos)
    {
        pos = std::fminf(std::fmaxf(pos, 0.0), 1.0);
        position = pos * (data->size()-1);
    }

    void setLooping(bool is_looping) { looped = is_looping; };
    void setReverse(bool reversed) { reverse = reversed; };
    Hz getSampleRate() { return sr; }
};

}
}
