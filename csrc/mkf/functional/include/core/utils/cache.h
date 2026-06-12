#pragma once

#include "core/types/resource_header.h"
#include <QByteArray>
#include <QString>
#include <vector>

class Cache {
public:
    Cache(const QString& filename);
    ~Cache();
    void init(const QString& filename);
    void clear();
    ResourceHeader getHeader(int index);
    QString getSignature(int index);
    QByteArray getResource(int index);
    size_t n();
    QString getFilename();

    static const QString& unknownSignature();

private:
    QString filename;
    std::vector<int32_t> offsets;
    std::vector<ResourceHeader> headers;
    std::vector<QString> signatures;
    std::vector<QByteArray> byteArrays;
    std::vector<bool> isLoaded;
};