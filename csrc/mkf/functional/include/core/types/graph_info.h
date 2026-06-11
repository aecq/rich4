#pragma once

#include <cstdint>

#pragma pack(push, 1)

struct GraphInfo {
    int16_t width;
    int16_t height;
    int16_t x;
    int16_t y;
    uint32_t gsize;
};

#pragma pack(pop)
