#include "Config.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

Config& Config::instance()
{
    static Config instance;
    return instance;
}

Config::Config()
{
    setDefaults();
}

Config::~Config()
{
}

void Config::setDefaults()
{
    // App默认值
    m_config["app.version"] = "v 0.1.0";
    m_config["app.title"] = "MapCloud Tiler";

    // Log默认值
    m_config["log.enable"] = false;
    m_config["log.file"] = "tiler.log";
    m_config["log.level"] = "info";

    // Output默认值
    m_config["output.format"] = "mbtiles";
    m_config["output.directory"] = "output";

    // Task默认值
    m_config["task.workers"] = 4;
    m_config["task.savepipe"] = 1;
    m_config["task.timedelay"] = 0;
    m_config["task.skipexisting"] = false;
    m_config["task.resume"] = false;

    // TileMap默认值
    m_config["tm.min"] = 0;
    m_config["tm.max"] = 20;
    m_config["tm.format"] = "jpg";
    m_config["tm.schema"] = "xyz";
}

bool Config::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open config file:" << filePath;
        return false;
    }

    QTextStream in(&file);
    QString currentSection;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // 跳过空行和注释
        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        // 处理节
        if (line.startsWith("[") && line.endsWith("]")) {
            currentSection = line.mid(1, line.length() - 2);
            // 处理数组节 [[lrs]]
            if (currentSection.startsWith("[") && currentSection.endsWith("]")) {
                currentSection = currentSection.mid(1, currentSection.length() - 2);
            }
            continue;
        }

        // 处理键值对
        int equalPos = line.indexOf('=');
        if (equalPos > 0) {
            QString key = line.left(equalPos).trimmed();
            QString value = line.mid(equalPos + 1).trimmed();

            // 移除引号
            if (value.startsWith('"') && value.endsWith('"')) {
                value = value.mid(1, value.length() - 2);
            }

            // 构造完整键名
            QString fullKey = currentSection.isEmpty() ? key : currentSection + "." + key;

            // 尝试转换为适当的类型
            if (value == "true") {
                m_config[fullKey] = true;
            } else if (value == "false") {
                m_config[fullKey] = false;
            } else {
                bool ok;
                int intValue = value.toInt(&ok);
                if (ok) {
                    m_config[fullKey] = intValue;
                } else {
                    m_config[fullKey] = value;
                }
            }
        }
    }

    file.close();
    return true;
}

QVariant Config::getValue(const QString& key, const QVariant& defaultValue) const
{
    return m_config.value(key, defaultValue);
}

QString Config::getString(const QString& key, const QString& defaultValue) const
{
    return m_config.value(key, defaultValue).toString();
}

int Config::getInt(const QString& key, int defaultValue) const
{
    return m_config.value(key, defaultValue).toInt();
}

bool Config::getBool(const QString& key, bool defaultValue) const
{
    return m_config.value(key, defaultValue).toBool();
}

void Config::setValue(const QString& key, const QVariant& value)
{
    m_config[key] = value;
}

Config::AppConfig Config::getAppConfig() const
{
    AppConfig config;
    config.version = getString("app.version");
    config.title = getString("app.title");
    return config;
}

Config::LogConfig Config::getLogConfig() const
{
    LogConfig config;
    config.enable = getBool("log.enable");
    config.file = getString("log.file");
    config.level = getString("log.level");
    return config;
}

Config::OutputConfig Config::getOutputConfig() const
{
    OutputConfig config;
    config.format = getString("output.format");
    config.directory = getString("output.directory");
    return config;
}

Config::TaskConfig Config::getTaskConfig() const
{
    TaskConfig config;
    config.workers = getInt("task.workers", 4);
    config.savePipe = getInt("task.savepipe", 1);
    config.timeDelay = getInt("task.timedelay", 0);
    config.skipExisting = getBool("task.skipexisting", false);
    config.resume = getBool("task.resume", false);
    return config;
}

Config::TileMapConfig Config::getTileMapConfig() const
{
    TileMapConfig config;
    config.name = getString("tm.name");
    config.min = getInt("tm.min");
    config.max = getInt("tm.max");
    config.format = getString("tm.format");
    config.schema = getString("tm.schema");
    config.json = getString("tm.json");
    config.url = getString("tm.url");
    return config;
}

QList<Config::LayerConfig> Config::getLayerConfigs() const
{
    QList<LayerConfig> layers;

    // 注意: 简化的TOML解析器不能完全处理 [[lrs]] 数组
    // 这里假设只有一个lrs配置，实际使用时可能需要更完善的TOML解析库
    LayerConfig layer;
    layer.min = getInt("lrs.min", 0);
    layer.max = getInt("lrs.max", 20);
    layer.geojson = getString("lrs.geojson");
    layer.url = getString("lrs.url");

    if (!layer.geojson.isEmpty()) {
        layers.append(layer);
    }

    return layers;
}
