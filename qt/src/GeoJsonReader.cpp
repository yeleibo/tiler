#include "GeoJsonReader.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <limits>

QVector<GeoGeometry> GeoJsonReader::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open GeoJSON file:" << filePath;
        return QVector<GeoGeometry>();
    }

    QByteArray data = file.readAll();
    file.close();

    QString jsonString = QString::fromUtf8(data);
    return parseGeoJson(jsonString);
}

QVector<GeoGeometry> GeoJsonReader::parseGeoJson(const QString& jsonString)
{
    QVector<GeoGeometry> geometries;

    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (doc.isNull()) {
        qWarning() << "Invalid JSON";
        return geometries;
    }

    QJsonObject root = doc.object();

    // 处理 FeatureCollection
    if (root.value("type").toString() == "FeatureCollection") {
        QJsonArray features = root.value("features").toArray();
        for (const QJsonValue& featureVal : features) {
            QJsonObject feature = featureVal.toObject();
            QJsonObject geomObj = feature.value("geometry").toObject();
            if (!geomObj.isEmpty()) {
                geometries.append(parseGeometry(geomObj));
            }
        }
    }
    // 处理 Feature
    else if (root.value("type").toString() == "Feature") {
        QJsonObject geomObj = root.value("geometry").toObject();
        if (!geomObj.isEmpty()) {
            geometries.append(parseGeometry(geomObj));
        }
    }
    // 处理直接的 Geometry
    else if (root.contains("type") && root.contains("coordinates")) {
        geometries.append(parseGeometry(root));
    }
    // 处理 OpenStreetMap Nominatim 格式
    else if (doc.isArray()) {
        QJsonArray array = doc.array();
        for (const QJsonValue& item : array) {
            QJsonObject obj = item.toObject();
            QJsonValue geojsonVal = obj.value("geojson");
            if (!geojsonVal.isUndefined()) {
                QJsonObject geomObj = geojsonVal.toObject();
                if (!geomObj.isEmpty()) {
                    geometries.append(parseGeometry(geomObj));
                }
            }
        }
    }

    return geometries;
}

GeoGeometry GeoJsonReader::parseGeometry(const QJsonObject& geomObj)
{
    GeoGeometry geom;
    QString type = geomObj.value("type").toString();
    QJsonArray coordinates = geomObj.value("coordinates").toArray();

    if (type == "Polygon") {
        geom.type = GeoGeometry::Polygon;
        QPolygonF polygon = parsePolygonCoordinates(coordinates);
        geom.polygons.append(polygon);
        geom.bounds = calculatePolygonBounds(polygon);
    }
    else if (type == "MultiPolygon") {
        geom.type = GeoGeometry::MultiPolygon;
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();

        for (const QJsonValue& polyVal : coordinates) {
            QJsonArray polyCoords = polyVal.toArray();
            QPolygonF polygon = parsePolygonCoordinates(polyCoords);
            geom.polygons.append(polygon);

            QRectF bounds = calculatePolygonBounds(polygon);
            minX = qMin(minX, bounds.left());
            minY = qMin(minY, bounds.top());
            maxX = qMax(maxX, bounds.right());
            maxY = qMax(maxY, bounds.bottom());
        }
        geom.bounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    }

    return geom;
}

QPolygonF GeoJsonReader::parsePolygonCoordinates(const QJsonArray& coords)
{
    QPolygonF polygon;

    // Polygon coordinates: [[[lon, lat], [lon, lat], ...]]
    // 取第一个环（外环）
    if (coords.size() > 0) {
        QJsonArray ring = coords[0].toArray();
        for (const QJsonValue& pointVal : ring) {
            QJsonArray point = pointVal.toArray();
            if (point.size() >= 2) {
                double lon = point[0].toDouble();
                double lat = point[1].toDouble();
                polygon.append(QPointF(lon, lat));
            }
        }
    }

    return polygon;
}

QRectF GeoJsonReader::calculatePolygonBounds(const QPolygonF& polygon)
{
    if (polygon.isEmpty()) {
        return QRectF();
    }

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    for (const QPointF& point : polygon) {
        minX = qMin(minX, point.x());
        minY = qMin(minY, point.y());
        maxX = qMax(maxX, point.x());
        maxY = qMax(maxY, point.y());
    }

    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

QRectF GeoJsonReader::calculateBounds(const QVector<GeoGeometry>& geometries)
{
    if (geometries.isEmpty()) {
        return QRectF();
    }

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    for (const GeoGeometry& geom : geometries) {
        QRectF bounds = geom.getBounds();
        minX = qMin(minX, bounds.left());
        minY = qMin(minY, bounds.top());
        maxX = qMax(maxX, bounds.right());
        maxY = qMax(maxY, bounds.bottom());
    }

    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}
