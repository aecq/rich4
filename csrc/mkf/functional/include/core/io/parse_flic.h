#pragma once

#include "core/io/parse.h"
#include "core/utils/flic.h"
#include <QByteArray>
#include <QImage>
#include <cstdint>
#include <vector>

class FLICResource : public flic::FileInterface
{
public:
    FLICResource(QByteArray bytes) : m_bytes(bytes) {}
    bool ok() const override { return 0 <= m_pos && m_pos < m_bytes.size(); }
    size_t tell() override { return m_pos; }
    void seek(size_t pos) override { m_pos = pos; }
    uint8_t read8() override { return ok() ? m_bytes[m_pos++] : 0; }
    void write8(uint8_t value) override { if (ok()) { m_bytes[m_pos++] = value; } }

private:
    QByteArray m_bytes;
    size_t m_pos = 0;
};

static inline std::vector<QImage> parseFLIC(const QByteArray& bytes) {
    std::vector<QImage> images;
    FLICResource resource(bytes);
    flic::Decoder decoder(&resource);
    flic::Header header;
    if (!decoder.readHeader(header)) {
        return images;
    }
    std::vector<uint8_t> buffer(header.width * header.height);
    flic::Frame frame;
    frame.pixels = buffer.data();
    frame.rowstride = header.width;
    for (int i = 0; i < header.frames; i++) {
        if (!decoder.readFrame(frame)) {
            break;
        }
        QImage img(header.width, header.height, QImage::Format_RGB888);
        for (int y = 0; y < header.height; y++) {
            uint8_t* pixelIndex = buffer.data() + y * header.width;
            uint8_t* scanline = img.scanLine(y);
            for (int x = 0; x < header.width; x++) {
                uint8_t idx = pixelIndex[x];
                flic::Color c = frame.colormap[idx];
                scanline[3 * x] = c.r;
                scanline[3 * x + 1] = c.g;
                scanline[3 * x + 2] = c.b;
            }
        }
        images.push_back(img);
    }
    return images;
}

static inline int parseFPS(const QByteArray& bytes) {
    FLICResource resource(bytes);
    flic::Decoder decoder(&resource);
    flic::Header header;
    if (!decoder.readHeader(header)) {
        return -1;
    }
    switch (parseInt16(bytes.mid(4, 2))) {
        case int16_t(0xAF11):
            return 70 / header.speed;
        case int16_t(0xAF12):
            return 1000 / header.speed;
    }
    return -1;
}
