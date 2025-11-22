#ifndef GEOJSONREADER_H
#define GEOJSONREADER_H

#include <QString>
#include <QVector>
#include <QPolygonF>
#include <QRectF>
#include <QJsonObject>
#include <QJsonArray>

// 简化的GeoJSON几何对象
struct GeoGeometry {
    enum Type {
        Point,
        LineString,
        Polygon,
        MultiPolygon
    };

    Type type;
    QVector<QPolygonF> polygons; // 存储所有多边形
    QRectF bounds; // 边界框

    QRectF getBounds() const { return bounds; }
};

class GeoJsonReader
{
public:
    // 从文件加载GeoJSON
    static QVector<GeoGeometry> loadFromFile(const QString& filePath);

    // 解析GeoJSON字符串
    static QVector<GeoGeometry> parseGeoJson(const QString& jsonString);

    // 计算边界框
    static QRectF calculateBounds(const QVector<GeoGeometry>& geometries);

private:
    static GeoGeometry parseGeometry(const QJsonObject& geomObj);
    static QPolygonF parsePolygonCoordinates(const QJsonArray& coords);
    static QRectF calculatePolygonBounds(const QPolygonF& polygon);
};

#endif // GEOJSONREADER_H
