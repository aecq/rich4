#pragma once

#include <cstdint>

#pragma pack(push, 1)

struct MapDataHeader {  // 地图数据头部（前40字节）
    uint32_t map_node_count;             // [0x00] 地图节点个数
    uint32_t map_node_array_offset;      // [0x04] 地图节点数组相对于资源首的偏移
    uint32_t land_node_count;            // [0x08] 普通土地节点个数
    uint32_t land_node_array_offset;     // [0x0C] 普通土地节点数组相对于资源首的偏移
    uint32_t facility_node_count;        // [0x10] 设施节点个数
    uint32_t facility_node_array_offset; // [0x14] 设施节点数组相对于资源首的偏移
    uint32_t commercial_node_count;      // [0x18] 上市企业节点个数
    uint32_t commercial_node_array_offset; // [0x1C] 上市企业节点数组相对于资源首的偏移
    uint32_t beauty_node_count;            // [0x20] 美观节点个数
    uint32_t beauty_node_array_offset;     // [0x24] 美观节点数组相对于资源首的偏移
};

struct MapNode {  // 大小: 40 字节 (0x28)
    int16_t x;                   // [0x00] X 坐标
    int16_t y;                   // [0x02] Y 坐标
    char name[20];               // [0x04] 地名
    int16_t neighbors[4];        // [0x18] 邻接点数组下标
    int16_t type;                // [0x20] 2000~3999: 普通土地 4000~5999: 设施; 6001~7999: 上市企业
    int16_t chunk;               // [0x22] Map 图像资源 chunk 下标
    int16_t special;             // [0x24] 特殊地点, 否则 0
    int16_t fork;                // [0x28] 0 环路点不含分叉点; 非 0 分叉点和非环路点
};

struct LandNode {  // 大小: 52 字节 (0x34)
    int16_t x;                   // [0x00] X 坐标
    int16_t y;                   // [0x02] Y 坐标
    char name[20];               // [0x04] 地名
    int8_t unknown0x18[3];       // [0x18] 未知字段
    int8_t face;                 // [0x1B] 朝向
    int16_t buy;                 // [0x1C] 购买价格
    int16_t upgrade;             // [0x1E] 升级价格
    int16_t toll[6];             // [0x20] 过路费
    char unknown0x2C[8];         // [0x2C] 未知字段
};

struct FacilityNode {  // 大小: 56 字节 (0x38)
    int16_t x;                   // [0x00] X 坐标
    int16_t y;                   // [0x02] Y 坐标
    char name[20];               // [0x04] 名称
    int8_t unknown0x18[3];       // [0x18] 未知字段
    int8_t face;                 // [0x1B] 朝向
    char unknown0x1C[6];         // [0x1C] 未知字段
    int16_t buy;                 // [0x22] 购买价格
    int16_t upgrade;             // [0x24] 升级价格
    int16_t toll[5];             // [0x26] 过路费
    char unknown0x30[8];         // [0x30] 未知字段
};

struct CommercialNode {  // 大小: 52 字节 (0x34)
    int16_t x;                   // [0x00] X 坐标
    int16_t y;                   // [0x02] Y 坐标
    char name[20];               // [0x04] 名称
    int8_t unknown0x18;          // [0x18] 未知字段
    int8_t unknown0x19;          // [0x19] 未知字段
    int8_t unknown0x1A;          // [0x1A] 未知字段
    int8_t unknown0x1B;          // [0x1B] 未知字段
    int32_t unknown0x1C;         // [0x1C] 未知字段
    int16_t sprite;              // [0x20] 图像资源偏移
    int16_t unknown0x22;         // [0x22] 未知字段
    int32_t unknown0x24;         // [0x24] 未知字段
    char unknown0x28[12];        // [0x28] 未知字段
};

struct BeautyNode {  // 大小: 28 字节 (0x1C)
    int16_t x;                   // [0x00] X 坐标
    int16_t y;                   // [0x02] Y 坐标
    char name[20];               // [0x04] 名称
    int16_t face;                // [0x18] 朝向
    int16_t sprite;              // [0x1A] 图像资源偏移
};

#pragma pack(pop)