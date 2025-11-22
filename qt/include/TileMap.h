#ifndef TILEMAP_H
#define TILEMAP_H

#include <QString>
#include "Tile.h"

// 瓦片地图类
class TileMap
{
public:
    TileMap();

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    QString description() const { return m_description; }
    void setDescription(const QString& desc) { m_description = desc; }

    QString schema() const { return m_schema; }
    void setSchema(const QString& schema) { m_schema = schema; }

    int minZoom() const { return m_minZoom; }
    void setMinZoom(int min) { m_minZoom = min; }

    int maxZoom() const { return m_maxZoom; }
    void setMaxZoom(int max) { m_maxZoom = max; }

    QString format() const { return m_format; }
    void setFormat(const QString& format) { m_format = format; }

    QString json() const { return m_json; }
    void setJson(const QString& json) { m_json = json; }

    QString url() const { return m_url; }
    void setUrl(const QString& url) { m_url = url; }

    QString token() const { return m_token; }
    void setToken(const QString& token) { m_token = token; }

    // 获取瓦片URL
    QString getTileUrl(const Tile& tile) const;
    QString getTileUrl(int z, int x, int y) const;

private:
    int m_id;
    QString m_name;
    QString m_description;
    QString m_schema;     // "xyz" or "tms"
    int m_minZoom;
    int m_maxZoom;
    QString m_format;     // "png", "jpg", "pbf", etc.
    QString m_json;       // TileJSON metadata
    QString m_url;        // URL template
    QString m_token;      // Access token if needed
};

#endif // TILEMAP_H
