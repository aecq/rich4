#include "core/utils/Cache.h"
#include "core/io/Parse.h"

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
        if (isCompressed(headers[i])) {
            // 为了确保能正确解压出前 4 字节数据，建议传入至少 16 字节 的压缩数据
            QByteArray data = parseCompressed(file.read(32), 4);
            // 取前 4 字节
            signatures[i] = QString(data.constData()).left(4);
        } else {
            signatures[i] = QString(file.read(4));
        }
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

ResourceHeader Cache::getHeader(int index) {
    return headers[index];
}

QString Cache::getSignature(int index) {
    if (signatures[index].startsWith("SPR") ||
        signatures[index].startsWith("SMP") ||
        signatures[index].startsWith("RIFF")
    ) {
        return signatures[index];
    }
    return QString("....");
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