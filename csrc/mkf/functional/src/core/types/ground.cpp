#include "core/types/ground.h"
#include "core/io/parse.h"

const int TILE_WIDTH = 32;
const int TILE_HEIGHT = 32;
const int TILE_SIZE = TILE_WIDTH * TILE_HEIGHT;

Ground::Ground(const QByteArray& bytes) {
    if (bytes.size() < sizeof(GroundHeader)) {
        return;
    }

    header = parseGroundHeader(bytes);

    int offset = sizeof(GroundHeader);

    palette = parsePalette(bytes, offset);
    offset += PALETTE_SIZE * sizeof(int16_t);

    indices.resize(header.count);
    if (offset + header.count * sizeof(uint16_t) <= bytes.size()) {
        memcpy(indices.data(), bytes.constData() + offset, header.count * sizeof(uint16_t));
    }
    offset += header.count * sizeof(uint16_t);

    tiles.resize(header.count);
    for (uint16_t i = 0; i < header.count; ++i) {
        QImage tile(TILE_WIDTH, TILE_HEIGHT, QImage::Format_Indexed8);
        tile.setColorTable(palette);
        if (offset + TILE_SIZE <= bytes.size()) {
            memcpy(tile.bits(), bytes.constData() + offset, TILE_SIZE);
        }
        tiles[i] = tile;
        offset += TILE_SIZE;
    }
}

GroundHeader Ground::getHeader() const {
    return header;
}

QVector<QRgb> Ground::getPalette() const {
    return palette;
}

QVector<uint16_t> Ground::getIndices() const {
    return indices;
}

QVector<QImage> Ground::getTiles() const {
    return tiles;
}

QImage Ground::getTile(uint16_t index) const {
    if (index < tiles.size()) {
        return tiles[index];
    }
    return QImage();
}