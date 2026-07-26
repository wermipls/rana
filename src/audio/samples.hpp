#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include "common.hpp"

namespace rana {
namespace audio {

struct DecodedSample {
    // fixme: this struct is a makeshift solution to keep old code working,
    // probably replace it with something else
    struct StereoPair {
        float l, r;
    };

    std::vector<StereoPair> data;
    float rate;
};

std::shared_ptr<DecodedSample> decode_flac(std::vector<uint8_t> s);
std::shared_ptr<DecodedSample> decode_opus(std::vector<uint8_t> s);

}
}
