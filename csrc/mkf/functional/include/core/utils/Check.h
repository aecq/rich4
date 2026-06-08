#pragma once

#include "core/types/ResourceHeader.h"
#include "core/types/SPRSMPHeader.h"
#include <QByteArray>
#include <QRegularExpression>

static inline bool guessBig5(QByteArray &data) {
    int printableCount = 0;
    int total = std::min(static_cast<int>(data.size()), 256);
    
    for (int i = 0; i < total; ++i) {
        uint8_t byte = static_cast<uint8_t>(data[i]);
        if ((byte >= 0x20 && byte <= 0x7E) || (byte >= 0x80 && byte <= 0xFF)) {
            printableCount++;
        }
    }
    
    if (total > 0 && (printableCount * 100 / total) > 80) {
        return true;
    }
    
    return false;
}

// 检查位置 i 的字节是否是 Big5 双字节字符的第二字节
static bool isBig5TrailingByte(const QByteArray& data, int i) {
    if (i <= 0) {
        return false;
    }
    unsigned char prev = static_cast<unsigned char>(data[i - 1]);
    return (prev >= 0x81 && prev <= 0xFE);
}

static inline bool isCompressed(const ResourceHeader& header) {
    return header.compressed_size != header.uncompressed_size;
}

static inline bool isSPR(const SPRSMPHeader& header) {
    return memcmp(header.signature, "SPR\0", sizeof(char[4])) == 0;
}

static QString legalFilename(const QString& filename) {
    return QString(filename).replace(QRegularExpression("[\\/:*?\"<>|]"), "_");
}