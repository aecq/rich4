#pragma once

#include <cstdint>

#pragma pack(push, 1)

struct ResourceHeader {
    uint32_t uncompressed_size;
    uint32_t compressed_size;
    uint32_t image_data_offset;
    uint32_t image_data_size;
};

#pragma pack(pop)
