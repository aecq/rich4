#pragma once

#include "core/types/GraphInfo.h"
#include "core/types/ResourceHeader.h"
#include "core/types/SPRSMPHeader.h"
#include "core/utils/Check.h"
#include <cstdint>
#include <windows.h>
#include <qdebug.h>
#include <qstringview.h>
#include <vector>
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QPixmap>

using ResourceIndexOffset = int32_t;
using ResourceOffset = int32_t;

extern "C" {
    void mkf_decompress(void *dst, const void *src, size_t bufsz);
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

// ====================
//  parseImages: 以 SPR 或 SMP 开头 const QByteArray& bytes 中解析图像块列表
// ====================
static inline std::vector<QImage> parseImages(const QByteArray& bytes, int offset=0) {
    std::vector<QImage> images;
    SPRSMPHeader header = parseSPRSMPHeader(bytes, offset);
    std::vector<GraphInfo> graphInfos = parseGraphInfos(bytes, offset + sizeof(SPRSMPHeader));
    images.reserve(header.num_chunks);
    if (isSPR(header)) {
        // palette: 512 bytes // each int16_t -> RGB555, 256 colors
        QVector<QRgb> palette(256);
        const int16_t* src = reinterpret_cast<const int16_t*>(bytes.constData() + header.start_offset);
        std::generate(palette.begin(), palette.end(), [&]() {
            return parseRGB555(*src++);
        });
        // (width[0] * height[0]) bytes  // QImage::Format_Indexed8
        // ...
        // (width[num_chunks-1] * height[num_chunks-1]) bytes  // QImage::Format_Indexed8
        for (auto& info : graphInfos) {
            QImage image = QImage(info.width, info.height, QImage::Format_Indexed8);
            image.setColorTable(palette);
            images.push_back(image);
        }
    } else {
        // (width[0] * height[0]) bytes  // QImage::Format_RGB555
        // ...
        // (width[num_chunks-1] * height[num_chunks-1]) bytes  // QImage::Format_RGB555
        for (auto& info : graphInfos) {
            QImage image = QImage(info.width, info.height, QImage::Format_RGB555);
            images.push_back(image);
        }
    }
    return images;
}

// ====================
//  parseBig5Simple(bytes, size): 从 const QByteArray& bytes 中解析 Big5 编码的文本
// ====================
static inline QString parseBig5Simple(const QByteArray& bytes, size_t size) {
    int wideChars = MultiByteToWideChar(950, 0, bytes, size, NULL, 0);
    if (wideChars > 0) {
        QVector<wchar_t> wideBuffer(wideChars);
        MultiByteToWideChar(950, 0, bytes, size, wideBuffer.data(), wideChars);
        return QString::fromWCharArray(wideBuffer.data(), wideChars);
    }
    return QString();
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
        qDebug("No data available");
        return output;
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
        return QString("(空)");
    } else {
        return output;
    }
}