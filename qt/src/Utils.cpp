#include "Utils.h"
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QDateTime>
#include <cmath>

QString Utils::generateShortId()
{
    const QString charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    QString result;
    for (int i = 0; i < 8; ++i) {
        int index = QRandomGenerator::global()->bounded(charset.length());
        result.append(charset[index]);
    }
    return result;
}

QString Utils::replaceTileUrl(const QString& urlTemplate, int z, int x, int y)
{
    QString url = urlTemplate;
    url.replace("{z}", QString::number(z));
    url.replace("{x}", QString::number(x));
    url.replace("{y}", QString::number(y));

    // 计算 -y (TMS坐标)
    int maxY = (1 << z) - 1;
    url.replace("{-y}", QString::number(maxY - y));

    return url;
}

int Utils::flipY(int z, int y)
{
    return (1 << z) - 1 - y;
}

bool Utils::createDirectory(const QString& path)
{
    QDir dir;
    return dir.mkpath(path);
}

bool Utils::fileExists(const QString& filePath)
{
    return QFileInfo::exists(filePath);
}

double Utils::getFileSize(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.size() / 1024.0; // 返回KB
}

QString Utils::formatTime(qint64 milliseconds)
{
    double seconds = milliseconds / 1000.0;
    return QString::number(seconds, 'f', 3) + "s";
}
