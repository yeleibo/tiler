#ifndef LAYER_H
#define LAYER_H

#include <QString>
#include <QVector>
#include <QRectF>
#include "GeoJsonReader.h"

// 图层类
class Layer
{
public:
    Layer();
    Layer(int zoom, const QString& url, const QString& geojsonPath);

    int zoom() const { return m_zoom; }
    void setZoom(int zoom) { m_zoom = zoom; }

    QString url() const { return m_url; }
    void setUrl(const QString& url) { m_url = url; }

    QString geojsonPath() const { return m_geojsonPath; }
    void setGeojsonPath(const QString& path);

    qint64 tileCount() const { return m_tileCount; }
    void setTileCount(qint64 count) { m_tileCount = count; }

    QVector<GeoGeometry> geometries() const { return m_geometries; }
    QRectF bounds() const { return m_bounds; }

    // 计算该层级的瓦片数量
    qint64 calculateTileCount();

    // 判断瓦片是否在范围内
    bool containsTile(int x, int y) const;

private:
    int m_zoom;
    QString m_url;
    QString m_geojsonPath;
    qint64 m_tileCount;
    QVector<GeoGeometry> m_geometries;
    QRectF m_bounds;

    // 计算瓦片与多边形是否相交
    bool tileIntersectsGeometry(int x, int y, const GeoGeometry& geom) const;
    QRectF getTileBounds(int x, int y) const;
};

#endif // LAYER_H
