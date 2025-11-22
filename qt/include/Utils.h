#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QPointF>
#include <QRectF>

class Utils
{
public:
    // 生成短ID
    static QString generateShortId();

    // URL模板替换
    static QString replaceTileUrl(const QString& urlTemplate, int z, int x, int y);

    // TMS坐标转换 (flipY)
    static int flipY(int z, int y);

    // 创建目录
    static bool createDirectory(const QString& path);

    // 检查文件是否存在
    static bool fileExists(const QString& filePath);

    // 获取文件大小（KB）
    static double getFileSize(const QString& filePath);

    // 格式化时间（毫秒转秒）
    static QString formatTime(qint64 milliseconds);
};

#endif // UTILS_H
