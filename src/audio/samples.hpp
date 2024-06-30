#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include "common.hpp"

namespace rana {
namespace audio {

struct DecodedSample {
    std::vector<SampleStereo> data;
    float rate;
};

std::shared_ptr<DecodedSample> decode_flac(std::vector<uint8_t> s);
std::shared_ptr<DecodedSample> decode_opus(std::vector<uint8_t> s);

}
}
