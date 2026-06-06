#pragma once

#include <cstdint>

#pragma pack(push, 1)

struct SPRSMPHeader {
    char signature[4];
    uint32_t num_chunks;
    uint32_t start_offset;
};

#pragma pack(pop)
