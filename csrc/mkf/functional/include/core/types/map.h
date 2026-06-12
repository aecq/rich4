#pragma once

#include <cstdint>

#pragma pack(push, 1)

struct MapDataHeader {  // 地图数据头部（前40字节）
    uint32_t map_node_count;             // [0x00] 地图节点个数
    uint32_t map_node_array_offset;      // [0x04] 地图节点数组相对于资源首的偏移
    int8_t unknown[32];    
};

struct MapNode {  // 大小: 40 字节 (0x28)
    int16_t x;                   // [0x00] X 坐标
    int16_t y;                   // [0x02] Y 坐标
    char n[20];                  // [0x04] 地名
    int16_t a;                   // [0x18] 邻 a 的数组下标
    int16_t b;                   // [0x1A] 邻 b 的数组下标
    int16_t c;                   // [0x1C] 邻 c 的数组下标
    int16_t d;                   // [0x1E] 邻 d 的数组下标
    int16_t t;                   // [0x20] 2000~3999: 普通土地 4000~5999: 设施; 6001~7999: 上市企业
    int16_t r;                   // [0x22] Map 图像资源 chunk 下标
    int16_t s;                   // [0x24] 特殊地点, 否则 00.
    int16_t f;                   // [0x28] 00 环路点; 否则支路点及其与环路交点
};

#pragma pack(pop)