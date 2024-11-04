#include "samples.hpp"

#include <cmath>
#include <stddef.h>
#include "log.hpp"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#include <opus.h>
#include <tracy/Tracy.hpp>

namespace rana {
namespace audio {

std::shared_ptr<DecodedSample> decode_flac(std::vector<uint8_t> s)
{
    ZoneScoped;
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

std::shared_ptr<DecodedSample> decode_opus(std::vector<uint8_t> s)
{
    ZoneScoped;
    int error;
    auto st = opus_decoder_create(48000, 2, &error);
    if (error != OPUS_OK) {
        log::err("failed to create opus decoder");
        return nullptr;
    }

    auto decoded = std::make_shared<DecodedSample>();
    auto &samples = decoded->data;
    decoded->rate = 48000;

    size_t samples_pos = 0;
    for (size_t i = 0; i < s.size(); ) {
        samples.resize(samples.size() + 960*6);
        int payload_bytes = s[i] << 8 | s[i+1];
        i += 2;
        if (!payload_bytes) {
            continue;
        }

        auto samples_decoded = opus_decode_float(
            st, 
            (uint8_t *)&s[i],
            payload_bytes,
            (float *)&samples[samples_pos],
            (samples.size() - samples_pos) * 2 * sizeof(float),
            0
        );

        if (!samples_decoded) {
            log::err("failed to decode opus packet");
            opus_decoder_destroy(st);
            return nullptr;
        }

        samples_pos += samples_decoded;
        i += payload_bytes;
    }

    opus_decoder_destroy(st);
    samples.resize(samples_pos);
    return decoded;
}

}
}
