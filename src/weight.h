#pragma once

#include "incbin.h"
#include <array>
#include <cstdint>

INCBIN(netWeight, "gem.bin");

namespace nnue {

struct Weight {
    int16_t fc1_weight[768 * 32];
    int16_t fc1_bias[32];
    int16_t fc2_weight[128];
    int16_t fc2_bias;
};

const Weight* weight = reinterpret_cast<const Weight*>(gnetWeightData);

} // namespace nnue