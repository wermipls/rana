#pragma once
#include <vector>
#include <cmath>
#include <stdint.h>
#include <stddef.h>
#include <memory>

#include "common.hpp"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#include "log.hpp"

namespace rana {
namespace audio {

struct DecodedSample {
    std::vector<SampleStereo> data;
    float rate;
};

std::shared_ptr<DecodedSample> decode_flac(std::vector<uint8_t> s)
{
    auto df = drflac_open_memory(s.data(), s.size(), NULL);
    auto frames = df->totalPCMFrameCount;
    auto channels = df->channels;
    auto buf = std::vector<float>(df->totalPCMFrameCount * df->channels);

    auto f = drflac_read_pcm_frames_f32(df, buf.size(), buf.data());
    drflac_close(df);

    auto decoded = std::make_unique<DecodedSample>();
    decoded->rate = df->sampleRate;

    auto &out = decoded->data;
    out.resize(f);

    if (channels == 1) {
        for (size_t i = 0; i < frames; i++) {
            out[i].l = buf[i];
            out[i].r = buf[i];
        }
    } else if (channels == 2) {
        for (size_t i = 0; i < frames; i++) {
            out[i].l = buf[i*2];
            out[i].r = buf[i*2+1];
        }
    } else {
        log::err("unsupported flac channel count: %d", df->channels);
        return nullptr;
    }

    return decoded;
};

}
}
