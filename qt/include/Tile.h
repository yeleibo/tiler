#ifndef TILE_H
#define TILE_H

#include <QByteArray>
#include <QString>
#include <cmath>

// 瓦片结构
struct Tile {
    int z;
    int x;
    int y;
    QByteArray data;

    Tile() : z(0), x(0), y(0) {}
    Tile(int zoom, int col, int row) : z(zoom), x(col), y(row) {}

    // TMS坐标转换 (flipY)
    int flipY() const {
        return (1 << z) - 1 - y;
    }

    bool isValid() const {
        if (z < 0 || z > 30) return false;
        int maxTile = 1 << z;
        return x >= 0 && x < maxTile && y >= 0 && y < maxTile;
    }
};

// 瓦片格式常量
namespace TileFormat {
    const QString GZIP = "gzip";
    const QString ZLIB = "zlib";
    const QString PNG = "png";
    const QString JPG = "jpg";
    const QString PBF = "pbf";
    const QString WEBP = "webp";
}

// 瓦片大小
const int TILE_SIZE = 256;

// 瓦片缩放级别范围
const int ZOOM_MIN = 0;
const int ZOOM_MAX = 20;

#endif // TILE_H
