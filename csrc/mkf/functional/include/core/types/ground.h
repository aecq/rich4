#pragma once

#include <QByteArray>
#include <QImage>
#include <QRect>
#include <QRgb>
#include <QVector>

struct GroundHeader {
  char signature[4];
  uint16_t column;
  uint16_t row;
  uint16_t count;
  uint16_t unknown0x06;
  uint32_t unknown0x08;
};

class Ground {
private:
    GroundHeader header;
    QVector<QRgb> palette;
    QVector<uint16_t> indices;
    QVector<QImage> tiles;

public:
    Ground(const QByteArray& bytes);
    GroundHeader getHeader() const;
    QVector<QRgb> getPalette() const;
    QVector<uint16_t> getIndices() const;
    QVector<QImage> getTiles() const;
    QImage getTile(uint16_t index) const;
    QImage stitchFull();
    QImage stitchRect(QRect slice) const;
};