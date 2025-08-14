#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QHostAddress>
#include <QFileSystemWatcher>
#include <QSet>
#include <QFile>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include "logmanager.h"

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
    void on_pushButton_clicked();
    void on_stopButton_clicked();
    void onDirectoryChanged(const QString &path);
    void startFileTransfer(); // 修正: 不再接受参数，从队列中取文件
    void onLogMessage(const QString &message);
    void updateStatistics();

private:
    // 文件状态枚举
    enum FileStatus {
        Pending,  // 待发送
        Success,  // 成功发送
        Failure   // 发送失败
    };

    Ui::MainWindow *ui;
    QFileSystemWatcher *myFileSystemWatcher;

    // 用于跟踪所有文件的状态
    QMap<QString, FileStatus> m_fileStatus;
    // 用于跟踪每个文件重试次数的映射
    QMap<QString, int> m_fileRetries;

    // 新增: 用于文件传输队列和状态管理
    QQueue<QString> m_pendingFiles;
    bool m_isSendingFile = false;

    // 用于文件传输进度的成员变量
    QElapsedTimer m_speedTimer;
    qint64 m_bytesWrittenTotal = 0;
    qint64 m_fileSize = 0;
};
#endif // MAINWINDOW_H
