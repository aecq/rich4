#ifndef MKF_STRUCTS_H
#define MKF_STRUCTS_H

#include <cstdint>

#pragma pack(push, 1)

// 资源头结构 (16 bytes)
struct ResourceHeader {
    uint32_t uncompressed_size;
    uint32_t compressed_size;
    uint32_t image_data_offset;
    uint32_t image_data_size;
};

// SPR/SMP 头部 (12 bytes)
struct GraphBundleHeader {
    char signature[4]; // "SPR\0" or "SMP\0"
    uint32_t num_chunks;
    uint32_t start_offset;
};

// 图像描述结构 (12 bytes)
struct GraphInfo {
    int16_t width;
    int16_t height;
    int16_t x;
    int16_t y;
    uint32_t gsize;
};

#pragma pack(pop)

// 外部解压函数声明 (由用户提供的库实现，这里仅声明)
extern "C" void mkf_decompress(void *dst, const void *src, size_t bufsz);

#endif // MKF_STRUCTS_H
