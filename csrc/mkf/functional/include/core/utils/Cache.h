#pragma once

#include "core/types/ResourceHeader.h"
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
    void getResource(int index, QByteArray& data);
    size_t n();
    QString getFilename();

private:
    QString filename;
    std::vector<int32_t> offsets;
    std::vector<ResourceHeader> headers;
    std::vector<QString> signatures;
    std::vector<QByteArray> byteArrays;
    std::vector<bool> isLoaded;
};