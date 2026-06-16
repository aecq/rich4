#include "core/utils/cache.h"
#include "core/io/parse.h"

#include <QFile>

Cache::Cache(const QString& filename) {
    init(filename);
}


Cache::~Cache() {
}

void Cache::init(const QString& filename) {
    clear();
    this->filename = filename;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open file for reading");
    }
    offsets = parseResourceOffsets(file);
    headers = parseResourceHeaders(file, offsets);
    byteArrays.resize(offsets.size());
    signatures.resize(offsets.size());
    isLoaded.resize(offsets.size());
    for (int i = 0; i < offsets.size(); i++) {
        byteArrays[i].clear();
        file.seek(offsets[i] + sizeof(ResourceHeader));
        // 为了确保能正确解压出前 4 字节数据，建议传入至少 16 字节 的压缩数据
        static const int SIGNATURE_COUNT = 8;
        signatures[i] = isCompressed(headers[i]) ? parseCompressed(file.read(4 * SIGNATURE_COUNT), SIGNATURE_COUNT) : file.read(SIGNATURE_COUNT);
        isLoaded[i] = false;
    }
}

void Cache::clear() {
    filename = QString();
    offsets.clear();
    headers.clear();
    byteArrays.clear();
    signatures.clear();
    isLoaded.clear();
}

ResourceOffset Cache::getOffset(int index) {
    return offsets[index];
}

ResourceHeader Cache::getHeader(int index) {
    return headers[index];
}

QString Cache::getSignature(int index) {
    QByteArray data = signatures[index];
    if (parseInt32(data.mid(0, 4)) == headers[index].uncompressed_size) {
        if (parseInt16(data.mid(4, 2)) == int16_t(0xAF12)) {
            return QString("FLC");
        }
    }
    QString signature = QString(data.constData()).left(3);
    if (signature.startsWith("SPR") || signature.startsWith("SMP") || signature.startsWith("GND")) {
        return signature;
    }
    if ( signature.startsWith("RIF")) {
        return QString("RIFF");
    }
    return unknownSignature();
}


QByteArray Cache::getResource(int index) {
    QByteArray data;
    if (isLoaded[index]) {
        data = byteArrays[index];
        return data;
    }
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open file for reading");
    }
    file.seek(offsets[index] + sizeof(ResourceHeader));
    if (headers[index].compressed_size != headers[index].uncompressed_size)
        data = parseCompressed(file.read(headers[index].compressed_size), headers[index].uncompressed_size);
    else {
        data = file.read(headers[index].uncompressed_size);
    }
    byteArrays[index] = data;
    isLoaded[index] = true;
    return data;
}

size_t Cache::n() {
    return offsets.size();
}

QString Cache::getFilename() {
    return filename;
}

const QString& Cache::unknownSignature() {
    static const QString unknown = QString("...");
    return unknown;
}