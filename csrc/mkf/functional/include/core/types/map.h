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
    char name[20];               // [0x04] 地名
    int16_t neighbors[4];        // [0x18] 邻接点数组下标
    int16_t type;                // [0x20]
    int16_t chunk;               // [0x22] Map 图像资源 chunk 下标
    int16_t special;             // [0x24] 特殊地点, 否则 0
    int16_t fork;                // [0x28] 0 环路点不含分叉点; 非 0 分叉点和非环路点
};

#pragma pack(pop)