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

QImage Ground::stitchFull() {
    return stitchRect(QRect(0, 0, header.column, header.row));
}

QImage Ground::stitchRect(QRect slice) const {
    if (slice.x() < 0 || slice.y() < 0 ||
        slice.x() + slice.width() > header.column ||
        slice.y() + slice.height() > header.row ||
        slice.width() <= 0 || slice.height() <= 0) {
        return QImage();
    }

    const int outWidth = slice.width() * TILE_WIDTH;
    const int outHeight = slice.height() * TILE_HEIGHT;
    QImage result(outWidth, outHeight, QImage::Format_Indexed8);
    result.setColorTable(palette);

    for (int gy = 0; gy < slice.height(); ++gy) {
        for (int gx = 0; gx < slice.width(); ++gx) {
            const int gridX = slice.x() + gx;
            const int gridY = slice.y() + gy;
            const int indexOffset = gridY * header.column + gridX;

            if (indexOffset >= static_cast<int>(indices.size())) {
                continue;
            }

            const uint16_t tileIndex = indices[indexOffset];
            if (tileIndex >= tiles.size()) {
                continue;
            }

            const QImage& tile = tiles[tileIndex];
            const int destX = gx * TILE_WIDTH;
            const int destY = gy * TILE_HEIGHT;

            for (int ty = 0; ty < TILE_HEIGHT; ++ty) {
                const uchar* srcLine = tile.constScanLine(ty);
                uchar* destLine = result.scanLine(destY + ty);
                memcpy(destLine + destX, srcLine, TILE_WIDTH);
            }
        }
    }

    return result;
}