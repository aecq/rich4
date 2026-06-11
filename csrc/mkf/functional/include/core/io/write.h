#pragma once

#include "core/io/parse.h"
#include "core/types/resource_header.h"

static inline void replaceBinary(QFile& inFile, QFile& outFile, const QByteArray& resource, const int index) {
    // check resource size
    if (resource.size() > 0xFFFFFFFF) {
        throw std::runtime_error("replaceResource: resource size out of range");
    }
    uint32_t resourceSize = resource.size();
    // check file open
    if (!inFile.isOpen() || !outFile.isOpen()) {
        throw std::runtime_error("replaceResource: file not open");
    }
    // // check is read only is write only
    // if (!inFile.isReadOnly() || !outFile.isWriteOnly()) {
    //     throw std::runtime_error("replaceResource: file not read only or write only");
    // }
    // read inFile resourceOffset
    inFile.seek(0); ResourceIndexOffset resourceIndexOffset = *reinterpret_cast<ResourceIndexOffset*>(inFile.read(sizeof(ResourceIndexOffset)).data());
    // read inFile prefixResources
    std::vector<ResourceOffset> resourceOffsets = parseResourceOffsets(inFile);
    // check index
    if (index < 0 || index >= resourceOffsets.size()) {
        throw std::out_of_range("replaceResource: index out of range");
    }
    inFile.seek(sizeof(ResourceIndexOffset));
    QByteArray prefixResources = inFile.read(resourceOffsets[index] - sizeof(ResourceIndexOffset));
    if (prefixResources.size() != resourceOffsets[index] - sizeof(ResourceIndexOffset)) {
        throw std::runtime_error("replaceResource: prefixResources size not match");
    }
    // read inFile suffixResources
    QByteArray suffixResources = QByteArray();
    if (index + 1 < resourceOffsets.size())
    {
        inFile.seek(resourceOffsets[index + 1]);
        suffixResources = inFile.read(resourceIndexOffset - resourceOffsets[index + 1]);
        if (suffixResources.size() != resourceIndexOffset - resourceOffsets[index + 1]) {
            throw std::runtime_error("replaceResource: suffixResources size not match");
        }
    }
    // resource header
    ResourceHeader newHeader = {
        resourceSize,
        resourceSize,
        0,  // ignore
        0,  // ignore
    };
    // read inFile resourceHeaders, compute difference
    std::vector<ResourceHeader> resourceHeaders = parseResourceHeaders(inFile, resourceOffsets);
    int difference = resource.size() - resourceHeaders[index].compressed_size;
    // compute new
    ResourceIndexOffset newResourceIndexOffset = resourceIndexOffset + difference;
    std::vector<ResourceOffset> newResourceOffsets(resourceOffsets.size());
    for (int i = 0; i <= index; i++) {
        newResourceOffsets[i] = resourceOffsets[i];
    }
    for (int i = index + 1; i < resourceOffsets.size(); i++) {
        newResourceOffsets[i] = resourceOffsets[i] + difference;
    }
    // write
    outFile.write(reinterpret_cast<const char*>(&newResourceIndexOffset), sizeof(ResourceIndexOffset));
    outFile.write(prefixResources);
    outFile.write(reinterpret_cast<const char*>(&newHeader), sizeof(ResourceHeader));
    outFile.write(resource);
    outFile.write(suffixResources);
    outFile.write(reinterpret_cast<const char*>(newResourceOffsets.data()), sizeof(ResourceOffset) * newResourceOffsets.size());
    outFile.flush();
    outFile.close();
}