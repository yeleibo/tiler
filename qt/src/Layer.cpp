#define _USE_MATH_DEFINES
#include "Layer.h"
#include <cmath>
#include <QPolygonF>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Layer::Layer()
    : m_zoom(0), m_tileCount(0)
{
}

Layer::Layer(int zoom, const QString& url, const QString& geojsonPath)
    : m_zoom(zoom), m_url(url), m_geojsonPath(geojsonPath), m_tileCount(0)
{
    if (!geojsonPath.isEmpty()) {
        setGeojsonPath(geojsonPath);
    }
}

void Layer::setGeojsonPath(const QString& path)
{
    m_geojsonPath = path;
    m_geometries = GeoJsonReader::loadFromFile(path);
    m_bounds = GeoJsonReader::calculateBounds(m_geometries);
}

qint64 Layer::calculateTileCount()
{
    if (m_geometries.isEmpty()) {
        // 如果没有几何体，计算整个级别的瓦片数
        qint64 numTiles = 1LL << m_zoom;
        m_tileCount = numTiles * numTiles;
        return m_tileCount;
    }

    // 简化计算：基于边界框估算瓦片数量
    // 实际应该使用tilecover算法精确计算
    int numTiles = 1 << m_zoom;

    // 经纬度转瓦片坐标
    auto lon2tile = [this](double lon) {
        return static_cast<int>(floor((lon + 180.0) / 360.0 * (1 << m_zoom)));
    };

    auto lat2tile = [this](double lat) {
        double latRad = lat * M_PI / 180.0;
        return static_cast<int>(floor((1.0 - asinh(tan(latRad)) / M_PI) / 2.0 * (1 << m_zoom)));
    };

    qint64 totalTiles = 0;
    for (const GeoGeometry& geom : m_geometries) {
        QRectF bounds = geom.getBounds();

        int minX = lon2tile(bounds.left());
        int maxX = lon2tile(bounds.right());
        int minY = lat2tile(bounds.top());
        int maxY = lat2tile(bounds.bottom());

        // 确保在有效范围内
        minX = qMax(0, qMin(minX, numTiles - 1));
        maxX = qMax(0, qMin(maxX, numTiles - 1));
        minY = qMax(0, qMin(minY, numTiles - 1));
        maxY = qMax(0, qMin(maxY, numTiles - 1));

        totalTiles += (qint64)(maxX - minX + 1) * (maxY - minY + 1);
    }

    m_tileCount = totalTiles;
    return m_tileCount;
}

bool Layer::containsTile(int x, int y) const
{
    if (m_geometries.isEmpty()) {
        return true;
    }

    for (const GeoGeometry& geom : m_geometries) {
        if (tileIntersectsGeometry(x, y, geom)) {
            return true;
        }
    }

    return false;
}

bool Layer::tileIntersectsGeometry(int x, int y, const GeoGeometry& geom) const
{
    // 简化判断：使用瓦片边界框与几何体边界框相交
    QRectF tileBounds = getTileBounds(x, y);
    QRectF geomBounds = geom.getBounds();

    return tileBounds.intersects(geomBounds);
}

QRectF Layer::getTileBounds(int x, int y) const
{
    int numTiles = 1 << m_zoom;

    // 瓦片X对应的经度
    double lon1 = x * 360.0 / numTiles - 180.0;
    double lon2 = (x + 1) * 360.0 / numTiles - 180.0;

    // 瓦片Y对应的纬度
    auto tile2lat = [this, numTiles](int ty) {
        double n = M_PI - 2.0 * M_PI * ty / numTiles;
        return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
    };

    double lat1 = tile2lat(y);
    double lat2 = tile2lat(y + 1);

    return QRectF(QPointF(lon1, qMin(lat1, lat2)), QPointF(lon2, qMax(lat1, lat2)));
}
