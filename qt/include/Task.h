#ifndef TASK_H
#define TASK_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSqlDatabase>
#include <QThreadPool>
#include <QMutex>
#include <QAtomicInt>
#include "TileMap.h"
#include "Layer.h"
#include "Tile.h"

class Task : public QObject
{
    Q_OBJECT

public:
    explicit Task(QObject *parent = nullptr);
    ~Task();

    // 设置任务参数
    void setId(const QString& id) { m_id = id; }
    void setName(const QString& name) { m_name = name; }
    void setDescription(const QString& desc) { m_description = desc; }
    void setTileMap(const TileMap& tileMap) { m_tileMap = tileMap; }
    void setLayers(const QVector<Layer>& layers) { m_layers = layers; }
    void setOutputFormat(const QString& format) { m_outputFormat = format; }
    void setOutputFile(const QString& file) { m_outputFile = file; }
    void setWorkerCount(int count) { m_workerCount = count; }
    void setSavePipeSize(int size) { m_savePipeSize = size; }
    void setTimeDelay(int delay) { m_timeDelay = delay; }
    void setSkipExisting(bool skip) { m_skipExisting = skip; }
    void setResume(bool resume) { m_resume = resume; }

    // 获取任务信息
    QString id() const { return m_id; }
    QString name() const { return m_name; }
    qint64 totalTiles() const { return m_totalTiles; }
    qint64 currentProgress() const { return m_currentProgress; }
    qint64 downloadedTiles() const { return m_downloadedTiles; }

    // 任务控制
    void start();
    void pause();
    void resume();
    void stop();

signals:
    void progressUpdated(qint64 current, qint64 total);
    void layerProgressUpdated(int zoom, qint64 current, qint64 total);
    void tileDownloaded(int z, int x, int y, qint64 size, qint64 timeMs);
    void layerCompleted(int zoom, qint64 count);
    void taskCompleted();
    void errorOccurred(const QString& error);
    void statusChanged(const QString& status);

private slots:
    void handleNetworkReply(QNetworkReply* reply);

private:
    // 初始化
    bool setupMBTilesDatabase();
    bool setupProgressDatabase();
    void calculateTotalTiles();

    // 下载相关
    void downloadNextLayer();
    void downloadTiles(const Layer& layer);
    void downloadTile(const Tile& tile, const QString& url);

    // 保存相关
    void saveTile(const Tile& tile);
    bool saveToMBTiles(const Tile& tile);
    bool saveToFile(const Tile& tile);

    // 检查相关
    bool tileExists(const Tile& tile);
    bool tileExistsInMBTiles(const Tile& tile);
    bool tileExistsInFile(const Tile& tile);
    bool isTileDownloaded(const Tile& tile);
    void markTileAsDownloaded(const Tile& tile);

    // 元数据
    QMap<QString, QString> getMetaItems();

    // 成员变量
    QString m_id;
    QString m_name;
    QString m_description;
    QString m_outputFile;
    QString m_outputFormat;

    TileMap m_tileMap;
    QVector<Layer> m_layers;

    qint64 m_totalTiles;
    QAtomicInt m_currentProgress;
    QAtomicInt m_downloadedTiles;

    qint64 m_currentLayerTiles;
    QAtomicInt m_currentLayerProgress;

    int m_workerCount;
    int m_savePipeSize;
    int m_timeDelay;
    bool m_skipExisting;
    bool m_resume;

    int m_currentLayerIndex;
    bool m_running;
    bool m_paused;

    QNetworkAccessManager* m_networkManager;
    QSqlDatabase m_db;
    QSqlDatabase m_progressDB;

    QMutex m_dbMutex;
    QMutex m_progressMutex;

    QMap<QNetworkReply*, Tile> m_pendingTiles;
    QMutex m_pendingMutex;

    QVector<Tile> m_tileQueue;
    int m_activeDownloads;
    QMutex m_queueMutex;

    void processQueue();
};

#endif // TASK_H
