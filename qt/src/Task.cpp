#include "Task.h"
#include "Utils.h"
#include "Config.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QNetworkRequest>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>

Task::Task(QObject *parent)
    : QObject(parent)
    , m_totalTiles(0)
    , m_currentProgress(0)
    , m_downloadedTiles(0)
    , m_currentLayerTiles(0)
    , m_currentLayerProgress(0)
    , m_workerCount(4)
    , m_savePipeSize(200)
    , m_timeDelay(0)
    , m_skipExisting(false)
    , m_resume(false)
    , m_currentLayerIndex(0)
    , m_running(false)
    , m_paused(false)
    , m_activeDownloads(0)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &Task::handleNetworkReply);

    m_id = Utils::generateShortId();
}

Task::~Task()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    if (m_progressDB.isOpen()) {
        m_progressDB.close();
    }
}

void Task::start()
{
    m_running = true;
    m_paused = false;
    m_currentLayerIndex = 0;
    m_currentProgress = 0;
    m_downloadedTiles = 0;

    emit statusChanged("Initializing...");

    // 计算总瓦片数
    calculateTotalTiles();

    // 初始化数据库或输出目录
    if (m_outputFormat == "mbtiles") {
        if (!setupMBTilesDatabase()) {
            emit errorOccurred("Failed to setup MBTiles database");
            m_running = false;
            return;
        }
    } else {
        // 设置输出目录
        if (m_outputFile.isEmpty()) {
            QString outDir = Config::instance().getString("output.directory", "output");
            m_outputFile = QString("%1/%2")
                            .arg(outDir).arg(m_name);
        }

        // 创建输出目录
        if (!Utils::createDirectory(m_outputFile)) {
            emit errorOccurred(QString("Failed to create output directory: %1").arg(m_outputFile));
            m_running = false;
            return;
        }
    }

    // 初始化进度数据库
    if (m_resume) {
        if (!setupProgressDatabase()) {
            qWarning() << "Failed to setup progress database, continuing without resume support";
            m_resume = false;
        }
    }

    emit statusChanged("Starting download...");

    // 开始下载
    downloadNextLayer();
}

void Task::pause()
{
    m_paused = true;
    emit statusChanged("Paused");
}

void Task::resume()
{
    m_paused = false;
    emit statusChanged("Resumed");
    processQueue();
}

void Task::stop()
{
    m_running = false;
    m_paused = false;
    emit statusChanged("Stopped");
}

void Task::calculateTotalTiles()
{
    m_totalTiles = 0;
    for (Layer& layer : m_layers) {
        qint64 count = layer.calculateTileCount();
        m_totalTiles += count;
        qDebug() << "Zoom:" << layer.zoom() << "Tiles:" << count;
    }

    qDebug() << "Total tiles:" << m_totalTiles;
}

void Task::downloadNextLayer()
{
    if (!m_running || m_paused) {
        return;
    }

    if (m_currentLayerIndex >= m_layers.size()) {
        // 所有层级下载完成
        emit taskCompleted();
        emit statusChanged("Task completed");
        m_running = false;
        return;
    }

    const Layer& layer = m_layers[m_currentLayerIndex];
    emit statusChanged(QString("Downloading zoom level %1...").arg(layer.zoom()));

    downloadTiles(layer);
}

void Task::downloadTiles(const Layer& layer)
{
    // 清空队列
    m_queueMutex.lock();
    m_tileQueue.clear();
    m_queueMutex.unlock();

    int numTiles = 1 << layer.zoom();
    qint64 skippedCount = 0;  // 已跳过的瓦片数

    // 构建瓦片队列
    for (int x = 0; x < numTiles; ++x) {
        for (int y = 0; y < numTiles; ++y) {
            // 检查瓦片是否在图层范围内
            if (!layer.containsTile(x, y)) {
                continue;
            }

            Tile tile(layer.zoom(), x, y);

            // 检查是否需要跳过（不更新进度，等后面统一调整）
            if (m_resume && isTileDownloaded(tile)) {
                skippedCount++;
                continue;
            }

            if (m_skipExisting && tileExists(tile)) {
                skippedCount++;
                if (m_resume) {
                    markTileAsDownloaded(tile);
                }
                continue;
            }

            // 添加到队列
            m_queueMutex.lock();
            m_tileQueue.append(tile);
            m_queueMutex.unlock();
        }
    }

    // 初始化当前层级进度（使用实际队列大小+已跳过数量）
    m_queueMutex.lock();
    m_currentLayerTiles = m_tileQueue.size() + skippedCount;
    m_queueMutex.unlock();
    m_currentLayerProgress = skippedCount;  // 已跳过的算作已完成

    // 调整总瓦片数：用实际值替换预估值
    qint64 estimatedCount = layer.tileCount();
    qint64 actualCount = m_currentLayerTiles;
    m_totalTiles = m_totalTiles - estimatedCount + actualCount;

    // 更新已跳过瓦片的总进度
    if (skippedCount > 0) {
        m_currentProgress.fetchAndAddOrdered(skippedCount);
        emit progressUpdated(m_currentProgress, m_totalTiles);
    }

    emit layerProgressUpdated(layer.zoom(), m_currentLayerProgress, m_currentLayerTiles);

    qDebug() << QString("Layer Z%1: Estimated=%2, Actual=%3, Skipped=%4, Queue=%5, TotalTiles=%6")
                .arg(layer.zoom()).arg(estimatedCount).arg(actualCount)
                .arg(skippedCount).arg(m_tileQueue.size()).arg(m_totalTiles);

    // 启动下载队列处理
    processQueue();
}

void Task::downloadTile(const Tile& tile, const QString& url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    request.setRawHeader("Referer", "https://map.tianditu.gov.cn");

    QNetworkReply* reply = m_networkManager->get(request);

    // 记录待处理的瓦片
    m_pendingMutex.lock();
    m_pendingTiles[reply] = tile;
    m_pendingMutex.unlock();
}

void Task::handleNetworkReply(QNetworkReply* reply)
{
    reply->deleteLater();

    // 获取对应的瓦片
    m_pendingMutex.lock();
    Tile tile = m_pendingTiles.value(reply);
    m_pendingTiles.remove(reply);
    m_pendingMutex.unlock();

    QElapsedTimer timer;
    timer.start();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Download error:" << reply->errorString();
        emit errorOccurred(QString("Failed to download tile (%1,%2,%3): %4")
                          .arg(tile.z).arg(tile.x).arg(tile.y).arg(reply->errorString()));
        m_activeDownloads--;
        QTimer::singleShot(0, this, &Task::processQueue);
        return;
    }

    // 读取数据
    tile.data = reply->readAll();

    if (tile.data.isEmpty()) {
        qWarning() << "Empty tile data";
        m_activeDownloads--;
        QTimer::singleShot(0, this, &Task::processQueue);
        return;
    }

    // 保存瓦片
    saveTile(tile);

    // 标记为已下载
    if (m_resume) {
        markTileAsDownloaded(tile);
    }

    // 更新进度
    m_currentProgress.fetchAndAddOrdered(1);
    m_downloadedTiles.fetchAndAddOrdered(1);
    m_currentLayerProgress.fetchAndAddOrdered(1);

    qint64 elapsed = timer.elapsed();
    emit tileDownloaded(tile.z, tile.x, tile.y, tile.data.size(), elapsed);
    emit progressUpdated(m_currentProgress, m_totalTiles);
    emit layerProgressUpdated(tile.z, m_currentLayerProgress, m_currentLayerTiles);

    qDebug() << QString("Tile(%1,%2,%3), %4ms, %.2f KB")
                .arg(tile.z).arg(tile.x).arg(tile.y)
                .arg(elapsed).arg(tile.data.size() / 1024.0);

    // 减少活跃下载计数，继续处理队列
    m_activeDownloads--;
    QTimer::singleShot(0, this, &Task::processQueue);
}

void Task::saveTile(const Tile& tile)
{
    if (m_outputFormat == "mbtiles") {
        saveToMBTiles(tile);
    } else {
        saveToFile(tile);
    }
}

bool Task::saveToMBTiles(const Tile& tile)
{
    QMutexLocker locker(&m_dbMutex);

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?, ?, ?, ?)");
    query.addBindValue(tile.z);
    query.addBindValue(tile.x);
    query.addBindValue(tile.flipY());
    query.addBindValue(tile.data);

    if (!query.exec()) {
        qWarning() << "Failed to save tile to MBTiles:" << query.lastError().text();
        return false;
    }

    return true;
}

bool Task::saveToFile(const Tile& tile)
{
    QString dir = QString("%1/%2/%3").arg(m_outputFile).arg(tile.z).arg(tile.x);
    if (!Utils::createDirectory(dir)) {
        return false;
    }

    QString fileName = QString("%1/%2.%3").arg(dir).arg(tile.y).arg(m_tileMap.format());
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file:" << fileName;
        return false;
    }

    file.write(tile.data);
    file.close();

    return true;
}

bool Task::tileExists(const Tile& tile)
{
    if (m_outputFormat == "mbtiles") {
        return tileExistsInMBTiles(tile);
    } else {
        return tileExistsInFile(tile);
    }
}

bool Task::tileExistsInMBTiles(const Tile& tile)
{
    QMutexLocker locker(&m_dbMutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?");
    query.addBindValue(tile.z);
    query.addBindValue(tile.x);
    query.addBindValue(tile.flipY());

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

bool Task::tileExistsInFile(const Tile& tile)
{
    QString fileName = QString("%1/%2/%3/%4.%5")
                          .arg(m_outputFile).arg(tile.z).arg(tile.x).arg(tile.y).arg(m_tileMap.format());
    return QFileInfo::exists(fileName);
}

bool Task::isTileDownloaded(const Tile& tile)
{
    if (!m_progressDB.isOpen()) {
        return false;
    }

    QMutexLocker locker(&m_progressMutex);

    QSqlQuery query(m_progressDB);
    query.prepare("SELECT COUNT(*) FROM downloaded_tiles WHERE z = ? AND x = ? AND y = ?");
    query.addBindValue(tile.z);
    query.addBindValue(tile.x);
    query.addBindValue(tile.y);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

void Task::markTileAsDownloaded(const Tile& tile)
{
    if (!m_progressDB.isOpen()) {
        return;
    }

    QMutexLocker locker(&m_progressMutex);

    QSqlQuery query(m_progressDB);
    query.prepare("INSERT OR IGNORE INTO downloaded_tiles (z, x, y) VALUES (?, ?, ?)");
    query.addBindValue(tile.z);
    query.addBindValue(tile.x);
    query.addBindValue(tile.y);

    if (!query.exec()) {
        qWarning() << "Failed to mark tile as downloaded:" << query.lastError().text();
    }
}

bool Task::setupMBTilesDatabase()
{
    QString dbPath = m_outputFile;
    if (dbPath.isEmpty()) {
        QString outDir = Config::instance().getString("output.directory", "output");
        Utils::createDirectory(outDir);
        dbPath = QString("%1/%2.mbtiles")
                    .arg(outDir).arg(m_name);
    }

    // 如果不跳过已存在的瓦片，删除旧数据库
    if (!m_skipExisting && QFileInfo::exists(dbPath)) {
        QFile::remove(dbPath);
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "mbtiles_" + m_id);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    // 创建表
    QSqlQuery query(m_db);

    query.exec("PRAGMA synchronous=OFF");
    query.exec("PRAGMA locking_mode=EXCLUSIVE");
    query.exec("PRAGMA journal_mode=DELETE");

    query.exec("CREATE TABLE IF NOT EXISTS tiles ("
               "zoom_level INTEGER, "
               "tile_column INTEGER, "
               "tile_row INTEGER, "
               "tile_data BLOB)");

    query.exec("CREATE TABLE IF NOT EXISTS metadata (name TEXT, value TEXT)");
    query.exec("CREATE UNIQUE INDEX IF NOT EXISTS name ON metadata (name)");
    query.exec("CREATE UNIQUE INDEX IF NOT EXISTS tile_index ON tiles(zoom_level, tile_column, tile_row)");

    // 插入元数据
    QMap<QString, QString> metaItems = getMetaItems();
    for (auto it = metaItems.begin(); it != metaItems.end(); ++it) {
        query.prepare("INSERT OR REPLACE INTO metadata (name, value) VALUES (?, ?)");
        query.addBindValue(it.key());
        query.addBindValue(it.value());
        query.exec();
    }

    m_outputFile = dbPath;
    return true;
}

bool Task::setupProgressDatabase()
{
    QString outDir = Config::instance().getString("output.directory", "output");
    Utils::createDirectory(outDir);

    QString progressPath = QString("%1/%2.progress.db")
                              .arg(outDir).arg(m_name);

    if (!m_resume && QFileInfo::exists(progressPath)) {
        QFile::remove(progressPath);
    }

    m_progressDB = QSqlDatabase::addDatabase("QSQLITE", "progress_" + m_id);
    m_progressDB.setDatabaseName(progressPath);

    if (!m_progressDB.open()) {
        qWarning() << "Failed to open progress database:" << m_progressDB.lastError().text();
        return false;
    }

    QSqlQuery query(m_progressDB);
    query.exec("CREATE TABLE IF NOT EXISTS downloaded_tiles ("
               "z INTEGER NOT NULL, "
               "x INTEGER NOT NULL, "
               "y INTEGER NOT NULL, "
               "downloaded_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
               "PRIMARY KEY (z, x, y))");

    query.exec("CREATE INDEX IF NOT EXISTS idx_tile ON downloaded_tiles(z, x, y)");

    return true;
}

QMap<QString, QString> Task::getMetaItems()
{
    QMap<QString, QString> meta;

    meta["id"] = m_id;
    meta["name"] = m_name;
    meta["description"] = m_description;
    meta["attribution"] = "<a href=\"http://www.atlasdata.cn/\" target=\"_blank\">&copy; MapCloud</a>";
    meta["basename"] = m_tileMap.name();
    meta["format"] = m_tileMap.format();
    meta["type"] = m_tileMap.schema();
    meta["pixel_scale"] = QString::number(TILE_SIZE);
    meta["version"] = "1.2";

    // 计算边界和中心（简化版）
    meta["bounds"] = "-180.0,-85.0,180.0,85.0";
    meta["center"] = "0.0,0.0," + QString::number((m_tileMap.minZoom() + m_tileMap.maxZoom()) / 2);
    meta["minzoom"] = QString::number(m_tileMap.minZoom());
    meta["maxzoom"] = QString::number(m_tileMap.maxZoom());

    if (!m_tileMap.json().isEmpty()) {
        meta["json"] = m_tileMap.json();
    }

    return meta;
}

void Task::processQueue()
{
    if (!m_running || m_paused) {
        return;
    }

    m_queueMutex.lock();
    int queueSize = m_tileQueue.size();
    m_queueMutex.unlock();

    // 如果队列为空且没有活跃下载，当前层级完成
    if (queueSize == 0 && m_activeDownloads == 0) {
        if (m_currentLayerIndex < m_layers.size()) {
            const Layer& layer = m_layers[m_currentLayerIndex];
            qint64 completedCount = m_currentLayerTiles;  // 使用实际的当前层级瓦片数
            m_currentLayerIndex++;
            emit layerCompleted(layer.zoom(), completedCount);

            // 继续下一层
            QTimer::singleShot(0, this, &Task::downloadNextLayer);
        }
        return;
    }

    // 启动新的下载直到达到并发限制
    while (m_activeDownloads < m_workerCount && queueSize > 0) {
        if (!m_running || m_paused) {
            break;
        }

        m_queueMutex.lock();
        if (m_tileQueue.isEmpty()) {
            m_queueMutex.unlock();
            break;
        }

        Tile tile = m_tileQueue.takeFirst();
        m_queueMutex.unlock();

        m_activeDownloads++;

        // 生成URL并下载
        const Layer& layer = m_layers[m_currentLayerIndex];
        QString url = layer.url().isEmpty() ? m_tileMap.getTileUrl(tile) : Utils::replaceTileUrl(layer.url(), tile.z, tile.x, tile.y);

        // 如果设置了延时，使用QTimer延时下载
        if (m_timeDelay > 0) {
            QTimer::singleShot(m_timeDelay, this, [this, tile, url]() {
                downloadTile(tile, url);
            });
        } else {
            downloadTile(tile, url);
        }

        m_queueMutex.lock();
        queueSize = m_tileQueue.size();
        m_queueMutex.unlock();
    }
}
