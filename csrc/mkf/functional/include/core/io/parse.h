#pragma once

#include "core/types/graph_info.h"
#include "core/types/ground.h"
#include "core/types/map.h"
#include "core/types/resource_header.h"
#include "core/types/spr_smp_header.h"
#include "core/utils/big5hkscs_table.h"
#include "core/utils/check.h"
#include "core/utils/flic.h"
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QPixmap>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

const int PALETTE_SIZE = 256;

using ResourceIndexOffset = int32_t;
using ResourceOffset = int32_t;

extern "C" {
    void mkf_decompress(void *dst, const void *src, size_t bufsz);
}

template <typename T>
static inline T readDataAtOffset(const QByteArray &byteArray, int offset)
{
    // 边界检查：防止偏移越界导致崩溃
    if (offset < 0 || offset + sizeof(T) > byteArray.size()) {
        qWarning() << "偏移量越界！offset:" << offset << "数据长度:" << byteArray.size();
        return T(); // 返回类型默认值
    }

    // 1. const_cast 去掉 const 限制（只读操作无风险）
    // 2. 指针 + offset 偏移到目标字节位置
    // 3. 强制转换为目标类型指针，再解引用取值
    return *reinterpret_cast<T*>(const_cast<char*>(byteArray.constData()) + offset);
}

// ====================
//  parseCompress: 从 const QByteArray& compressedBytes 中解析压缩数据
// ====================
static inline QByteArray parseCompressed(const QByteArray& compressedBytes, int uncompressedBufferSize) {
    QByteArray bytes;
    if (uncompressedBufferSize < 0) {
        return bytes;
    }
    bytes.resize(uncompressedBufferSize);
    mkf_decompress(bytes.data(), compressedBytes.constData(), uncompressedBufferSize);
    return bytes;
}

// ====================
//  parseInt16: 从 const QByteArray& bytes 中解析 int16_t 值
// ====================
static inline int16_t parseInt16(const QByteArray& bytes, int offset=0) {
    int16_t value;
    memcpy(&value, bytes.constData() + offset, sizeof(int16_t));
    return value;
}

// ====================
//  parseInt32: 从 const QByteArray& bytes 中解析 int32_t 值
// ====================
static inline int32_t parseInt32(const QByteArray& bytes, int offset=0) {
    int32_t value;
    memcpy(&value, bytes.constData() + offset, sizeof(int32_t));
    return value;
}

// ====================
//  parseResourceOffsets: 从文件 QFile& file 中解析资源索引列表
// ====================
static inline std::vector<ResourceOffset> parseResourceOffsets(QFile& file) {
    // 初始化
    std::vector<ResourceOffset> offsets;
    // 读取资源索引列表首地址
    file.seek(0);
    ResourceIndexOffset resourceIndexOffset = parseInt32(file.read(sizeof(ResourceIndexOffset)));
    // 检查资源索引列表字节数是否是 sizeof(ResourceOffset) 的整数倍
    if ((file.size() - resourceIndexOffset) % sizeof(ResourceOffset) != 0) {
        qDebug("Resource offsets size in bytes (%d) is not a multiple of sizeof(ResourceOffset) (%d)", 
            file.size() - resourceIndexOffset, sizeof(ResourceOffset));
        return offsets;
    }
    // 预分配空间
    offsets.reserve((file.size() - resourceIndexOffset) / sizeof(ResourceOffset));
    // 读取资源索引列表到文件尾
    file.seek(resourceIndexOffset);
    while (!file.atEnd()) { 
        offsets.push_back(parseInt32(file.read(sizeof(ResourceOffset))));
    }
    // 返回资源索引列表
    return offsets;
}

// ====================
//  parseResourceHeader: 从 const QByteArray& bytes 中解析资源头部信息
// ====================
static inline ResourceHeader parseResourceHeader(const QByteArray& bytes, int offset=0) {
    ResourceHeader header;
    memcpy(&header, bytes.constData() + offset, sizeof(ResourceHeader));
    return header;
}

// ====================
//  parseResourceHeaders: 从文件 QFile& file 中解析资源头部信息
// ====================
static inline std::vector<ResourceHeader> parseResourceHeaders(QFile& file, const std::vector<ResourceOffset>& offsets) {
    std::vector<ResourceHeader> headers;
    headers.reserve(offsets.size());
    for (auto& offset : offsets) {
        file.seek(offset);
        ResourceHeader header;
        memcpy(&header, file.read(sizeof(ResourceHeader)), sizeof(ResourceHeader));
        headers.push_back(header);
    }
    return headers;
}

// ====================
//  parseSPRSMPHeader: 从 const QByteArray& bytes 中解析 SPRSMP 头部信息
// ====================
static inline SPRSMPHeader parseSPRSMPHeader(const QByteArray& bytes, int offset=0) {
    SPRSMPHeader header;
    memcpy(&header, bytes.constData() + offset, sizeof(SPRSMPHeader));
    return header;
}

// ====================
//  parseGraphInfo: 从 const QByteArray& bytes 中解析图像块信息
// ====================
static inline GraphInfo parseGraphInfo(const QByteArray& bytes, int offset=0) {
    GraphInfo info;
    memcpy(&info, bytes.constData() + offset, sizeof(GraphInfo));
    return info;
}

// ====================
//  parseGraphInfos: 以 SPR 或 SMP 开头 const QByteArray& bytes 中解析图像块信息列表
// ====================
static inline std::vector<GraphInfo> parseGraphInfos(const QByteArray& bytes, int offset=0) {
    std::vector<GraphInfo> chunks;
    SPRSMPHeader header = parseSPRSMPHeader(bytes, offset);
    chunks.reserve(header.num_chunks);
    for (int i = 0; i < header.num_chunks; i++) {
        chunks.push_back(parseGraphInfo(bytes, offset + sizeof(SPRSMPHeader) + i * sizeof(GraphInfo)));
    }
    
    return chunks;
}

// ====================
//  parseRGB555: 从 int16_t 按 RGB555 解析为 QRgb
// ====================
static inline QRgb parseRGB555(const int16_t& color) {
    uint8_t r = ((color >> 10) & 0x1F) * 255 / 31;
    uint8_t g = ((color >> 5) & 0x1F) * 255 / 31;
    uint8_t b = (color & 0x1F) * 255 / 31;
    return qRgb(r, g, b);
}

static inline QVector<QRgb> parsePalette(const QByteArray& bytes, int offset=0) {
    QVector<QRgb> palette(PALETTE_SIZE);
    for (int i = 0; i < palette.size(); i++) {
        palette[i] = parseRGB555(readDataAtOffset<int16_t>(bytes, offset + i * sizeof(int16_t)));
    }
    return palette;
}

static inline QImage stretchGrayRange(const QImage& src, uint8_t minV, uint8_t maxV)
{
    if (src.format() != QImage::Format_Grayscale8)
        return src;

    // 防止分母0（全同色）
    if (minV == maxV)
        return src.copy();

    QImage dst(src.size(), QImage::Format_Grayscale8);
    const uchar* srcBits = src.bits();
    uchar* dstBits = dst.bits();
    int srcBpl = src.bytesPerLine();
    int dstBpl = dst.bytesPerLine();
    int w = src.width();
    int h = src.height();

    float scale = 255.0f / (maxV - minV);

    for (int y = 0; y < h; ++y)
    {
        const uchar* srcLine = srcBits + y * srcBpl;
        uchar* dstLine = dstBits + y * dstBpl;
        for (int x = 0; x < w; ++x)
        {
            float val = (srcLine[x] - minV) * scale;
            dstLine[x] = static_cast<uchar>(qBound(0.0f, val, 255.0f));
        }
    }
    return dst;
}

// ====================
//  parseImage: 从 const QByteArray& bytes 中解析图像块
// ====================
static inline QImage parseImage(const QByteArray& bytes, const int& width, const int& height, QImage::Format format = QImage::Format_RGB555, bool stretchGray=false) {
    QImage image(width, height, format);
    if (format == QImage::Format_RGB555) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int index = y * width + x;
                const int colorOffset = index * sizeof(int16_t);
                QRgb color = parseRGB555(readDataAtOffset<int16_t>(bytes, colorOffset));
                image.setPixel(QPoint(x, y), color);
            }
        }
    } else if (format == QImage::Format_Grayscale8) {
        uint8_t minV = 255;
        uint8_t maxV = 0;
        memcpy(image.bits(), bytes.constData(), width * height);
        for (int i = 0; i < bytes.size(); i++) {
            uint8_t v = bytes[i];
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
        }
        if (stretchGray) {
            image = stretchGrayRange(image, minV, maxV);
        }
    }
    return image;
}

// ====================
//  parseImages: 以 SPR 或 SMP 开头 const QByteArray& bytes 中解析图像块列表
// ====================
static inline std::vector<QImage> parseImages(const QByteArray& bytes, int offset=0) {
    std::vector<QImage> images;
    SPRSMPHeader header = parseSPRSMPHeader(bytes, offset);
    std::vector<GraphInfo> graphInfos = parseGraphInfos(bytes, offset);
    images.reserve(header.num_chunks);
    if (isSPR(header)) {
        // palette: 512 bytes // each int16_t -> RGB555, 256 colors
        QVector<QRgb> palette = parsePalette(bytes, header.start_offset);
        // (width[0] * height[0]) bytes  // QImage::Format_Indexed8
        // ...
        // (width[num_chunks-1] * height[num_chunks-1]) bytes  // QImage::Format_Indexed8
        int start = 0;
        for (auto& info : graphInfos) {
            QImage image = QImage(info.width, info.height, QImage::Format_Indexed8);
            image.setColorTable(palette);
            int paletteEnd = header.start_offset + sizeof(int16_t) * PALETTE_SIZE;
            for (int y = 0; y < info.height; y++) {
                for (int x = 0; x < info.width; x++) {
                    int index = y * info.width + x;
                    int colorOffset = start + index;
                    uint colorIndex = (uint)(unsigned char)bytes[paletteEnd + colorOffset];
                    if (colorIndex < PALETTE_SIZE) {
                        image.setPixel(QPoint(x, y), colorIndex);
                    }
                }
            }
            images.push_back(image);
            start += info.gsize;
        }
    } else {
        // (width[0] * height[0]) bytes  // QImage::Format_RGB555
        // ...
        // (width[num_chunks-1] * height[num_chunks-1]) bytes  // QImage::Format_RGB555
        int start = 0;
        for (auto& info : graphInfos) {
            QImage image = parseImage(bytes.mid(header.start_offset + start), info.width, info.height);
            images.push_back(image);
            start += info.gsize;
        }
    }
    return images;
}

// ====================
//  indexOfNull: 查找 const QByteArray& bytes 中最后一个双字节 null 字符 (0x00 0x00) 的索引
// ====================
static inline int indexOfNull(const QByteArray& bytes) {
    if (bytes.isEmpty()) {
        return -1;
    }
    if (bytes.size() & 1) {
        return -1;
    }
    int i = bytes.size();
    for (; i >= 2 && bytes[i - 2] == 0 && bytes[i - 1] == 0; i -= 2) { /* dummy */ }
    return i;
}

static void appendUtf8(std::string& s, uint32_t cp)
{
    if (cp <= 0x7F)
        s += static_cast<char>(cp);
    else if (cp <= 0x7FF)
    {
        s += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0xFFFF)
    {
        s += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else
    {
        s += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
        s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

static inline std::string decodeBig5hkscsToUtf8(const uint8_t* data, size_t len)
{
    std::string out;
    out.reserve(len * 4);
    const auto& map = getBig5HKSCSTable();
    size_t i = 0;
    while (i < len)
    {
        uint8_t b1 = data[i++];
        if (b1 < 0x80)
        {
            out.push_back(static_cast<char>(b1));
            continue;
        }
        if (i >= len) break;
        uint8_t b2 = data[i++];
        uint16_t code = (static_cast<uint16_t>(b1) << 8) | b2;
        if (code >= 0xFEFF || map[code] == 0) {
            uint32_t rep = 0xFFFD;
            appendUtf8(out, rep);
            continue;
        }
        const auto& entry = map[code];
        appendUtf8(out, entry);
    }
    return out;
}

// ====================
//  parseBig5Simple(bytes, size): 从 const QByteArray& bytes 中解析 Big5 编码的文本
// ====================
static inline QString parseBig5Simple(const QByteArray& bytes, size_t size) {
    if (size <= 0 || bytes == nullptr) {
        return QString();
    }
    std::string utf8 = decodeBig5hkscsToUtf8(reinterpret_cast<const uint8_t*>(bytes.constData()), size);
    return QString::fromStdString(utf8);
}

// ====================
//  parseBig5Trim(bytes): 从 const QByteArray& bytes 中解析 Big5 编码的文本，自动移除尾随 null 字符
// ====================
static inline QString parseBig5Trim(const QByteArray& bytes) {
    return parseBig5Simple(bytes, indexOfNull(bytes));
}

// ====================
//  parseBig5Simple(bytes): 从 const QByteArray& bytes 中解析 Big5 编码的文本
// ====================
static inline QString parseBig5Simple(const QByteArray& bytes) {
    return parseBig5Simple(bytes, bytes.size());
}

// ====================
//  parseBig5: 从 const QByteArray& bytes 中解析 Big5 编码的文本
// ====================
static inline QString parseBig5(const QByteArray& bytes) {
    QString output;

    if (bytes.isEmpty()) {
        return QString("(空)");
    }
    
    int lineStart = 0;
    int i = 0;
    int pageIndex = 0;
    
    while (i < bytes.size()) {
        unsigned char byte = static_cast<unsigned char>(bytes[i]);
        
        if (byte == 0x00) {
            // 行分隔符
            if (i > lineStart) {
                QString line = parseBig5Simple(bytes.constData() + lineStart, i - lineStart);
                if (!line.isEmpty()) {
                    output += line + "\n";
                }
            }
            lineStart = i + 1;
            i++;
        } else if (byte == 0x40) {
            // 可能是分页符 '@'，也可能是 Big5 双字节字符的一部分
            if (isBig5TrailingByte(bytes, i)) {
                // 是 Big5 字符的一部分，跳过
                i++;
                continue;
            }
            // 是分页符
            if (i > lineStart) {
                QString line = parseBig5Simple(bytes.constData() + lineStart, i - lineStart);
                if (!line.isEmpty()) {
                    output += line + "\n";
                }
            }
            // 添加分页标记
            pageIndex++;
            output += "\n--- 第" + QString::number(pageIndex + 1) + "页 ---\n\n";
            lineStart = i + 1;
            i++;
        } else {
            // Big5 第一字节，跳过下一个字节
            if (byte >= 0x81 && byte <= 0xFE && i + 1 < bytes.size()) {
                i += 2;
            } else {
                i++;
            }
        }
    }
    
    // 处理最后一行
    if (lineStart < bytes.size()) {
        QString line = parseBig5Simple(bytes.constData() + lineStart, bytes.size() - lineStart);
        if (!line.isEmpty()) {
            output += line + "\n";
        }
    }
    
    if (output.isEmpty()) {
        return QString("(无可识别文本)");
    } else {
        return output;
    }
}

// ====================
//  parseGroundHeader(bytes, offset): 从 const QByteArray& bytes 中解析 Ground Header
// ====================
static inline GroundHeader parseGroundHeader(const QByteArray& bytes, int offset=0) {
    return readDataAtOffset<GroundHeader>(bytes, offset);
}

// ====================
//  parseGround(bytes, offset): 从 const QByteArray& bytes 中解析完整的 Ground 数据
// ====================
static inline Ground parseGround(const QByteArray& bytes, int offset=0) {
    return Ground(bytes.mid(offset));
}

// ====================
//  parseMapNodes(bytes, offset): 从 const QByteArray& bytes 中解析 MapNode 数组
// ====================
static inline std::vector<MapNode> parseMapNodes(const QByteArray& bytes, int offset=0) {
    MapDataHeader header = readDataAtOffset<MapDataHeader>(bytes, offset);
    int n = header.map_node_count;
    std::vector<MapNode> nodes(n);
    memcpy(nodes.data(), bytes.constData() + offset + header.map_node_array_offset, n * sizeof(MapNode));
    return nodes;
}

static inline std::vector<LandInfo> parseLandInfos(const QByteArray& bytes, int offset=0) {
    MapDataHeader header = readDataAtOffset<MapDataHeader>(bytes, offset);
    int n = header.land_info_count;
    std::vector<LandInfo> infos(n);
    memcpy(infos.data(), bytes.constData() + offset + header.land_info_array_offset, n * sizeof(LandInfo));
    return infos;
}

static inline std::vector<FacilityInfo> parseFacilityInfos(const QByteArray& bytes, int offset=0) {
    MapDataHeader header = readDataAtOffset<MapDataHeader>(bytes, offset);
    int n = header.facility_info_count;
    std::vector<FacilityInfo> infos(n);
    memcpy(infos.data(), bytes.constData() + offset + header.facility_info_array_offset, n * sizeof(FacilityInfo));
    return infos;
}

static inline std::vector<CommercialInfo> parseCommercialInfos(const QByteArray& bytes, int offset=0) {
    MapDataHeader header = readDataAtOffset<MapDataHeader>(bytes, offset);
    int n = header.commercial_info_count;
    std::vector<CommercialInfo> infos(n);
    memcpy(infos.data(), bytes.constData() + offset + header.commercial_info_array_offset, n * sizeof(CommercialInfo));
    return infos;
}

static inline std::vector<BeautyInfo> parseBeautyInfos(const QByteArray& bytes, int offset=0) {
    MapDataHeader header = readDataAtOffset<MapDataHeader>(bytes, offset);
    int n = header.beauty_info_count;
    std::vector<BeautyInfo> infos(n);
    memcpy(infos.data(), bytes.constData() + offset + header.beauty_info_array_offset, n * sizeof(BeautyInfo));
    return infos;
}

// ====================
//  parseWXH(string, width, height, errorMsg): 从形如 .wxh 的 QString 字符串中解析正整数 w 和 h
// ====================
static inline std::pair<GraphInfo, QString> parseWXH(const QString& string) {
    GraphInfo info;
    int indexOfX = string.indexOf("x");
    if (indexOfX == -1) {
        QString errorMsg = "Missing 'x' in string format";
        return std::make_pair(info, errorMsg);
    }
    QString widthStr = string.mid(1, indexOfX - 1);
    QString heightStr = string.mid(indexOfX + 1);
    info.width = widthStr.toUInt();
    info.height = heightStr.toUInt();
    if (info.width <= 0 || info.height <= 0) {
        QString errorMsg = QString("Invalid dimensions: %1x%2").arg(info.width).arg(info.height);
        return std::make_pair(info, errorMsg);
    }
    return std::make_pair(info, QString());
}