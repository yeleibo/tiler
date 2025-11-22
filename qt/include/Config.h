#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QVariant>
#include <QMap>

class Config
{
public:
    static Config& instance();

    // 加载配置文件
    bool loadFromFile(const QString& filePath);

    // 获取配置值
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    QString getString(const QString& key, const QString& defaultValue = QString()) const;
    int getInt(const QString& key, int defaultValue = 0) const;
    bool getBool(const QString& key, bool defaultValue = false) const;

    // 设置配置值
    void setValue(const QString& key, const QVariant& value);

    // 应用配置
    struct AppConfig {
        QString version;
        QString title;
    };

    struct LogConfig {
        bool enable;
        QString file;
        QString level;
    };

    struct OutputConfig {
        QString format;
        QString directory;
    };

    struct TaskConfig {
        int workers;
        int savePipe;
        int timeDelay;
        bool skipExisting;
        bool resume;
    };

    struct TileMapConfig {
        QString name;
        int min;
        int max;
        QString format;
        QString schema;
        QString json;
        QString url;
    };

    struct LayerConfig {
        int min;
        int max;
        QString geojson;
        QString url;
    };

    // 获取结构化配置
    AppConfig getAppConfig() const;
    LogConfig getLogConfig() const;
    OutputConfig getOutputConfig() const;
    TaskConfig getTaskConfig() const;
    TileMapConfig getTileMapConfig() const;
    QList<LayerConfig> getLayerConfigs() const;

private:
    Config();
    ~Config();
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    QMap<QString, QVariant> m_config;
    void setDefaults();
};

#endif // CONFIG_H
