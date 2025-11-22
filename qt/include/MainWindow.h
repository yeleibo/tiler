#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Task.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // UI事件槽
    void on_btnBrowseConfig_clicked();
    void on_btnLoadConfig_clicked();
    void on_btnBrowseGeoJson_clicked();
    void on_comboBoxUrlPreset_currentIndexChanged(int index);
    void on_btnStart_clicked();
    void on_btnPause_clicked();
    void on_btnStop_clicked();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();

    // 任务事件槽
    void onProgressUpdated(qint64 current, qint64 total);
    void onLayerProgressUpdated(int zoom, qint64 current, qint64 total);
    void onTileDownloaded(int z, int x, int y, qint64 size, qint64 timeMs);
    void onLayerCompleted(int zoom, qint64 count);
    void onTaskCompleted();
    void onErrorOccurred(const QString& error);
    void onStatusChanged(const QString& status);

private:
    void setupUI();
    void loadConfig(const QString& configFile);
    void updateUIFromConfig();
    void appendLog(const QString& message);
    void setControlsEnabled(bool enabled);

    Ui::MainWindow *ui;
    Task* m_task;
    QString m_configFile;
    QTime m_startTime;
    qint64 m_downloadedCount;
};

#endif // MAINWINDOW_H
