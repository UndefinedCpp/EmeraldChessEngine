#pragma once

#include "incbin.h"
#include <array>
#include <cstdint>

INCBIN(netWeight, "weight.bin");

using InputVector = std::array<int8_t, 768>;

namespace nnue {

struct Weight {
    uint8_t fc1_weight[16 * 768];
    uint8_t fc1_bias[16];
    uint8_t fc2_weight[16 * 16];
    uint8_t fc2_bias[16];
    uint8_t fc3_weight[16];
    uint8_t fc3_bias[1];
};

const Weight* weight = reinterpret_cast<const Weight*>(gnetWeightData);

} // namespace nnue