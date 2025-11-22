#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Config.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTime>
#include <QDateTime>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_task(nullptr)
    , m_downloadedCount(0)
{
    ui->setupUi(this);
    setupUI();
}

MainWindow::~MainWindow()
{
    if (m_task) {
        m_task->stop();
        delete m_task;
    }
    delete ui;
}

void MainWindow::setupUI()
{
    // 设置窗口图标等
    setWindowTitle("MapCloud Tiler - 地图瓦片下载器 v0.1.0");

    // 默认加载conf.toml
    m_configFile = "conf.toml";
    ui->lineEditConfigFile->setText(m_configFile);

    // 尝试自动加载配置
    if (QFile::exists(m_configFile)) {
        loadConfig(m_configFile);
    }
}

void MainWindow::on_btnBrowseConfig_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "选择配置文件",
        "",
        "TOML配置文件 (*.toml);;所有文件 (*)");

    if (!fileName.isEmpty()) {
        m_configFile = fileName;
        ui->lineEditConfigFile->setText(fileName);
    }
}

void MainWindow::on_btnLoadConfig_clicked()
{
    QString configFile = ui->lineEditConfigFile->text();
    if (configFile.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择配置文件");
        return;
    }

    loadConfig(configFile);
}

void MainWindow::on_btnBrowseGeoJson_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "选择GeoJSON文件",
        "",
        "GeoJSON文件 (*.geojson *.json);;所有文件 (*)");

    if (!fileName.isEmpty()) {
        ui->lineEditGeoJson->setText(fileName);
    }
}

void MainWindow::loadConfig(const QString& configFile)
{
    if (!Config::instance().loadFromFile(configFile)) {
        QMessageBox::warning(this, "警告", "无法加载配置文件: " + configFile);
        appendLog("无法加载配置文件: " + configFile);
        return;
    }

    m_configFile = configFile;
    updateUIFromConfig();
    appendLog("配置文件加载成功: " + configFile);
}

void MainWindow::on_comboBoxUrlPreset_currentIndexChanged(int index)
{
    QString url;
    switch (index) {
    case 0: // 自定义
        // 不修改URL
        return;
    case 1: // Google地图
        url = "http://mt0.google.com/vt/lyrs=m&x={x}&y={y}&z={z}";
        break;
    case 2: // Google卫星
        url = "http://mt0.google.com/vt/lyrs=s&x={x}&y={y}&z={z}";
        break;
    case 3: // 天地图矢量
        url = "https://t0.tianditu.gov.cn/DataServer?T=vec_w&x={x}&y={y}&l={z}&tk=75f0434f240669f4a2df6359275146d2";
        break;
    case 4: // 天地图卫星
        url = "https://t0.tianditu.gov.cn/DataServer?T=img_w&x={x}&y={y}&l={z}&tk=75f0434f240669f4a2df6359275146d2";
        break;
    default:
        return;
    }
    ui->lineEditUrl->setText(url);
}

void MainWindow::updateUIFromConfig()
{
    auto taskConfig = Config::instance().getTaskConfig();
    auto tmConfig = Config::instance().getTileMapConfig();
    auto outputConfig = Config::instance().getOutputConfig();
    auto layerConfigs = Config::instance().getLayerConfigs();

    // 从配置文件加载项目名称
    ui->lineEditProjectName->setText(tmConfig.name);
    ui->lineEditUrl->setText(tmConfig.url);
    ui->spinBoxMinZoom->setValue(tmConfig.min);
    ui->spinBoxMaxZoom->setValue(tmConfig.max);
    ui->spinBoxWorkers->setValue(taskConfig.workers);
    ui->spinBoxDelay->setValue(taskConfig.timeDelay);
    ui->checkBoxSkipExisting->setChecked(taskConfig.skipExisting);
    ui->checkBoxResume->setChecked(taskConfig.resume);

    // 从第一个layer加载GeoJSON路径（如果存在）
    if (!layerConfigs.isEmpty() && !layerConfigs.first().geojson.isEmpty()) {
        ui->lineEditGeoJson->setText(layerConfigs.first().geojson);
    }

    if (outputConfig.format == "mbtiles") {
        ui->comboBoxFormat->setCurrentIndex(0);
    } else {
        ui->comboBoxFormat->setCurrentIndex(1);
    }
}

void MainWindow::on_btnStart_clicked()
{
    // 检查项目名称
    QString projectName = ui->lineEditProjectName->text().trimmed();
    if (projectName.isEmpty()) {
        projectName = "my-map";
    }

    // 检查URL
    QString url = ui->lineEditUrl->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入或选择瓦片URL");
        return;
    }

    // 创建新的下载任务
    if (m_task) {
        delete m_task;
    }

    m_task = new Task(this);

    // 设置TileMap
    TileMap tileMap;
    auto tmConfig = Config::instance().getTileMapConfig();
    tileMap.setName(projectName);
    tileMap.setMinZoom(ui->spinBoxMinZoom->value());
    tileMap.setMaxZoom(ui->spinBoxMaxZoom->value());
    tileMap.setFormat(tmConfig.format);
    tileMap.setSchema(tmConfig.schema);
    tileMap.setJson(tmConfig.json);
    tileMap.setUrl(url);

    m_task->setTileMap(tileMap);
    m_task->setName(projectName);

    // 设置Layers
    QVector<Layer> layers;
    auto layerConfigs = Config::instance().getLayerConfigs();
    QString geojsonPath = ui->lineEditGeoJson->text().trimmed();

    if (layerConfigs.isEmpty()) {
        // 如果没有配置图层，使用全局设置
        for (int z = ui->spinBoxMinZoom->value(); z <= ui->spinBoxMaxZoom->value(); ++z) {
            Layer layer;
            layer.setZoom(z);
            layer.setUrl(url);
            // 如果UI中指定了GeoJSON，使用它
            if (!geojsonPath.isEmpty()) {
                layer.setGeojsonPath(geojsonPath);
            }
            layers.append(layer);
        }
    } else {
        // 使用配置的图层
        for (const auto& lc : layerConfigs) {
            for (int z = lc.min; z <= lc.max; ++z) {
                Layer layer;
                layer.setZoom(z);
                layer.setUrl(lc.url.isEmpty() ? url : lc.url);
                // 优先使用图层配置的GeoJSON，否则使用UI中的
                if (!lc.geojson.isEmpty()) {
                    layer.setGeojsonPath(lc.geojson);
                } else if (!geojsonPath.isEmpty()) {
                    layer.setGeojsonPath(geojsonPath);
                }
                layers.append(layer);
            }
        }
    }

    m_task->setLayers(layers);

    // 设置任务参数
    m_task->setOutputFormat(ui->comboBoxFormat->currentText());
    m_task->setWorkerCount(ui->spinBoxWorkers->value());
    m_task->setTimeDelay(ui->spinBoxDelay->value());
    m_task->setSkipExisting(ui->checkBoxSkipExisting->isChecked());
    m_task->setResume(ui->checkBoxResume->isChecked());

    // 连接信号
    connect(m_task, &Task::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_task, &Task::layerProgressUpdated, this, &MainWindow::onLayerProgressUpdated);
    connect(m_task, &Task::tileDownloaded, this, &MainWindow::onTileDownloaded);
    connect(m_task, &Task::layerCompleted, this, &MainWindow::onLayerCompleted);
    connect(m_task, &Task::taskCompleted, this, &MainWindow::onTaskCompleted);
    connect(m_task, &Task::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_task, &Task::statusChanged, this, &MainWindow::onStatusChanged);

    // 更新UI
    setControlsEnabled(false);
    ui->btnPause->setEnabled(true);
    ui->btnStop->setEnabled(true);
    ui->progressBar->setValue(0);
    m_downloadedCount = 0;
    m_startTime = QTime::currentTime();

    appendLog("========== 开始下载任务 ==========");
    appendLog(QString("项目名称: %1").arg(projectName));
    appendLog(QString("瓦片URL: %1").arg(url));
    if (!geojsonPath.isEmpty()) {
        appendLog(QString("限制范围: %1").arg(geojsonPath));
    }
    appendLog(QString("缩放级别: %1 - %2").arg(ui->spinBoxMinZoom->value()).arg(ui->spinBoxMaxZoom->value()));
    appendLog(QString("并发数: %1").arg(ui->spinBoxWorkers->value()));

    // 开始下载
    m_task->start();
}

void MainWindow::on_btnPause_clicked()
{
    if (!m_task) return;

    if (ui->btnPause->text() == "暂停") {
        m_task->pause();
        ui->btnPause->setText("继续");
        appendLog("任务已暂停");
    } else {
        m_task->resume();
        ui->btnPause->setText("暂停");
        appendLog("任务已继续");
    }
}

void MainWindow::on_btnStop_clicked()
{
    if (!m_task) return;

    m_task->stop();
    setControlsEnabled(true);
    ui->btnPause->setEnabled(false);
    ui->btnStop->setEnabled(false);
    ui->btnPause->setText("暂停");

    appendLog("任务已停止");
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "关于",
        "<h3>MapCloud Tiler</h3>"
        "<p>版本: 0.1.0</p>"
        "<p>地图瓦片下载器</p>"
        "<p>基于Qt 5.15.2开发</p>"
        "<p>&copy; 2024 MapCloud</p>");
}

void MainWindow::onProgressUpdated(qint64 current, qint64 total)
{
    if (total > 0) {
        int percentage = (current * 100) / total;
        ui->progressBar->setValue(percentage);

        QString status = QString("总进度: %1/%2 (%3%)")
                            .arg(current)
                            .arg(total)
                            .arg(percentage);
        ui->labelProgress->setText(status);
    }
}

void MainWindow::onLayerProgressUpdated(int zoom, qint64 current, qint64 total)
{
    if (total > 0) {
        int percentage = (current * 100) / total;
        QString status = QString("当前级别 Z%1: %2/%3 (%4%)")
                            .arg(zoom)
                            .arg(current)
                            .arg(total)
                            .arg(percentage);
        ui->statusBar->showMessage(status);
    }
}

void MainWindow::onTileDownloaded(int z, int x, int y, qint64 size, qint64 timeMs)
{
    m_downloadedCount++;

    // 每100个瓦片输出一次日志，避免日志过多
    if (m_downloadedCount % 100 == 0) {
        QString msg = QString("已下载 %1 个瓦片 (最近: z=%2, x=%3, y=%4, %5 KB, %6 ms)")
                         .arg(m_downloadedCount)
                         .arg(z)
                         .arg(x)
                         .arg(y)
                         .arg(size / 1024.0, 0, 'f', 2)
                         .arg(timeMs);
        appendLog(msg);
    }
}

void MainWindow::onLayerCompleted(int zoom, qint64 count)
{
    QString msg = QString("缩放级别 %1 下载完成，共 %2 个瓦片").arg(zoom).arg(count);
    appendLog(msg);
}

void MainWindow::onTaskCompleted()
{
    setControlsEnabled(true);
    ui->btnPause->setEnabled(false);
    ui->btnStop->setEnabled(false);
    ui->btnPause->setText("暂停");

    QTime endTime = QTime::currentTime();
    int elapsed = m_startTime.msecsTo(endTime);
    double seconds = elapsed / 1000.0;

    QString msg = QString("========== 任务完成 ==========\n总共下载: %1 个瓦片\n耗时: %2 秒")
                     .arg(m_downloadedCount)
                     .arg(seconds, 0, 'f', 3);
    appendLog(msg);

    QMessageBox::information(this, "完成", "下载任务已完成！");
}

void MainWindow::onErrorOccurred(const QString& error)
{
    appendLog("错误: " + error);
}

void MainWindow::onStatusChanged(const QString& status)
{
    ui->labelProgress->setText(status);
}

void MainWindow::appendLog(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    ui->textEditLog->append(QString("[%1] %2").arg(timestamp).arg(message));

    // 滚动到底部
    QTextCursor cursor = ui->textEditLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textEditLog->setTextCursor(cursor);
}

void MainWindow::setControlsEnabled(bool enabled)
{
    ui->btnBrowseConfig->setEnabled(enabled);
    ui->btnLoadConfig->setEnabled(enabled);
    ui->lineEditUrl->setEnabled(enabled);
    ui->comboBoxFormat->setEnabled(enabled);
    ui->spinBoxMinZoom->setEnabled(enabled);
    ui->spinBoxMaxZoom->setEnabled(enabled);
    ui->spinBoxWorkers->setEnabled(enabled);
    ui->spinBoxDelay->setEnabled(enabled);
    ui->checkBoxSkipExisting->setEnabled(enabled);
    ui->checkBoxResume->setEnabled(enabled);
    ui->btnStart->setEnabled(enabled);
}
