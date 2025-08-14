#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include "logmanager.h"

// 定义重试常量
const int MAX_RETRIES = 5;
const int RETRY_DELAY_MS = 2000;

QString ipAddress = "127.0.0.1";
quint16 port = 65432;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->ipAddressLineEdit->setPlaceholderText("请输入 IP 地址");
    ui->portLineEdit->setPlaceholderText("请输入端口号");

    // 连接 LogManager 的信号到 MainWindow 的槽函数
    connect(&LogManager::instance(), &LogManager::logMessage, this, &MainWindow::onLogMessage);

    // 实例化 QFileSystemWatcher 对象
    myFileSystemWatcher = new QFileSystemWatcher(this);

    // 连接 QFileSystemWatcher 的信号到自定义的槽函数
    connect(myFileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onDirectoryChanged);

    // 确保UI中的标签已初始化，初始值为0
    updateStatistics();

    // 注册 qint64 类型，以便在信号和槽中使用
    qRegisterMetaType<qint64>("qint64");

    // 显式连接按钮的点击事件到槽函数，并使用 UniqueConnection 确保只连接一次
    // 请确保UI文件中按钮的objectName分别为"pushButton"和"stopButton"
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked, Qt::UniqueConnection);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::on_stopButton_clicked, Qt::UniqueConnection);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// “开始监控”按钮的槽函数
void MainWindow::on_pushButton_clicked()
{
    // 获取用户输入的IP地址和端口号
    ipAddress = ui->ipAddressLineEdit->text();
    port = ui->portLineEdit->text().toUShort();

    // 检查IP地址和端口号是否有效
    if (ipAddress.isEmpty() || port == 0) {
        qDebug() << "请正确填写IP地址与端口号！";
        return;
    }

    QString folderPath = "E:/AIR/小长ISAR/实时数据回传/data";
    // 检查路径是否已在监控列表中，以防止重复添加
    if (!myFileSystemWatcher->directories().contains(folderPath)) {
        if (QDir(folderPath).exists()) {
            myFileSystemWatcher->addPath(folderPath);
            qDebug() << "已成功添加监控路径：" << folderPath;
        } else {
            qDebug() << "错误：指定的监控路径不存在：" << folderPath;
            QMessageBox::warning(this, "警告", "指定的监控文件夹不存在。");
        }
    } else {
        return; // 如果路径已在监控中，则立即返回，不再执行后续代码
    }

    // 扫描一次文件夹，检查并发送所有新文件
    QDir dir(folderPath);
    if (dir.exists()) {
        QStringList allFiles = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QString &fileName : allFiles) {
            QString filePath = dir.filePath(fileName);
            // 检查文件是否已存在于状态映射中，如果不存在，则为新文件
            if (!m_fileStatus.contains(filePath)) {
                m_fileStatus[filePath] = Pending; // 添加新文件，状态为待发送
                m_pendingFiles.enqueue(filePath); // 将文件路径加入发送队列
            }
        }
    }

    updateStatistics(); // 立即更新统计数据
    startFileTransfer(); // 启动文件传输队列
}

// “停止监控”按钮的槽函数
void MainWindow::on_stopButton_clicked()
{
    // 如果没有路径在监控中，直接返回，防止重复执行和警告
    if (myFileSystemWatcher->directories().isEmpty()) {
        return;
    }

    qDebug() << "停止监控文件夹...";
    myFileSystemWatcher->removePaths(myFileSystemWatcher->directories());
}

// 处理文件夹内容变化的槽函数
void MainWindow::onDirectoryChanged(const QString &path)
{
    QDir dir(path);
    QStringList allFiles = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    for (const QString &fileName : allFiles) {
        QString filePath = dir.filePath(fileName);
        // 检查文件是否已存在于状态映射中，如果不存在，则为新文件
        if (!m_fileStatus.contains(filePath)) {
            m_fileStatus[filePath] = Pending; // 添加新文件，状态为待发送
            m_pendingFiles.enqueue(filePath); // 将文件路径加入发送队列
        }
    }

    updateStatistics(); // 立即更新统计数据
    startFileTransfer(); // 启动文件传输队列
}

// 封装了文件传输和重试逻辑的槽函数
void MainWindow::startFileTransfer()
{
    // 如果正在发送文件或者队列为空，则不执行任何操作
    if (m_isSendingFile || m_pendingFiles.isEmpty()) {
        return;
    }

    m_isSendingFile = true; // 标记正在发送文件
    QString filePath = m_pendingFiles.dequeue(); // 从队列中取出下一个文件

    // 获取当前文件的重试次数
    int currentRetry = m_fileRetries.value(filePath, 0);

    // 检查是否超出最大重试次数
    if (currentRetry >= MAX_RETRIES) {
        qDebug() << "\033[31m文件" << QFileInfo(filePath).fileName() << "传输失败：已达到最大重试次数，放弃传输。\033[0m";
        m_fileRetries.remove(filePath); // 清除重试记录
        m_fileStatus[filePath] = Failure; // 将文件状态标记为失败
        updateStatistics(); // 更新统计数据
        m_isSendingFile = false; // 传输结束
        startFileTransfer(); // 尝试发送下一个文件
        return;
    }

    // 为每个文件创建一个独立的连接
    QTcpSocket* socket = new QTcpSocket(this);
    QFile* file = new QFile(filePath);

    // 尝试打开文件
    if (!file->open(QIODevice::ReadOnly)) {
        m_fileRetries.insert(filePath, currentRetry + 1); // 增加重试次数
        delete file;
        socket->deleteLater();
        m_isSendingFile = false; // 传输结束
        QTimer::singleShot(RETRY_DELAY_MS, [this, filePath](){
            // 将文件重新放回队列，等待下次发送
            m_pendingFiles.enqueue(filePath);
            startFileTransfer();
        });
        return;
    }

    // 记录文件大小和启动计时器
    m_fileSize = file->size();
    m_bytesWrittenTotal = 0;
    m_speedTimer.start();

    // 成功打开文件，设置连接信号
    connect(socket, &QTcpSocket::connected, this, [=]() {
        if (ui->label_currentFile) {
            ui->label_currentFile->setText(QString("正在发送: %1").arg(QFileInfo(filePath).fileName()));
        }

        // 构造并发送文件头
        QByteArray fileNameBytes = QFileInfo(*file).fileName().toUtf8();
        qint32 fileNameLength = fileNameBytes.size();
        qint64 fileSize = file->size();
        QByteArray fileSizeHeader = QString("%1").arg(fileSize, 16, 10, QChar(' ')).toUtf8();

        QByteArray outputBlock;
        outputBlock.append(reinterpret_cast<const char*>(&fileNameLength), sizeof(qint32));
        outputBlock.append(fileNameBytes);
        outputBlock.append(fileSizeHeader);

        socket->write(outputBlock);
    });

    // 新增：更新进度条和速度
    connect(socket, &QTcpSocket::bytesWritten, this, [=](qint64 bytes) {
        m_bytesWrittenTotal += bytes;
        qint64 totalBytes = m_fileSize;

        if (totalBytes > 0) {
            int percentage = (static_cast<double>(m_bytesWrittenTotal) / totalBytes) * 100;
            if (ui->progressBar) {
                ui->progressBar->setValue(percentage);
            }

            double elapsedTime = m_speedTimer.elapsed() / 1000.0;
            if (elapsedTime > 0) {
                double speed = m_bytesWrittenTotal / (1024.0 * 1024.0 * elapsedTime);
                if (ui->label_speed) {
                    ui->label_speed->setText(QString("%1 MB/s").arg(speed, 0, 'f', 2));
                }
            }
        }

        if (file->pos() < totalBytes) {
            qint64 bytesToWrite = totalBytes - file->pos();
            qint64 chunk = qMin(bytesToWrite, (qint64)64 * 1024);
            QByteArray outputBlock = file->read(chunk);
            socket->write(outputBlock);
        }
    });

    connect(socket, &QTcpSocket::readyRead, this, [=]() {
        QByteArray response = socket->readAll();
        if (response == "SUCCESS") {
            qDebug() << "\033[32m服务器确认文件" << file->fileName() << "接收成功。\033[0m";
            m_fileRetries.remove(filePath); // 成功后清除重试记录
            m_fileStatus[filePath] = Success; // 将文件状态标记为成功
            updateStatistics(); // 更新统计数据
        } else if (response == "FAILURE") {
            qDebug() << "\033[31m服务器返回失败，文件" << file->fileName() << "未成功接收。\033[0m";
            m_fileStatus[filePath] = Failure; // 将文件状态标记为失败
            updateStatistics();
        } else {
            qDebug() << "接收到未知服务器响应：" << response;
        }
        // 无论成功与否，收到服务器响应后都断开连接
        socket->disconnectFromHost();
    });

    // 处理连接错误并加入重试机制
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, [=](QAbstractSocket::SocketError socketError) {
        Q_UNUSED(socketError);
        // 如果是连接拒绝，并且没有达到最大重试次数，则进行重试
        if (socketError == QAbstractSocket::ConnectionRefusedError && m_fileRetries.value(filePath, 0) < MAX_RETRIES) {
            m_fileRetries.insert(filePath, m_fileRetries.value(filePath, 0) + 1); // 增加重试次数
            file->close();
            delete file;
            socket->deleteLater();
            m_isSendingFile = false; // 传输结束
            QTimer::singleShot(RETRY_DELAY_MS, [this, filePath](){
                // 将文件重新放回队列，等待下次发送
                m_pendingFiles.enqueue(filePath);
                startFileTransfer();
            });
        } else {
            qDebug() << "\033[31m连接错误：" << socket->errorString() << "，已达到最大重试次数，放弃传输。\033[0m";
            file->close();
            delete file;
            socket->disconnectFromHost();
        }
    });

    connect(socket, &QTcpSocket::disconnected, this, [=]() {
        // 文件发送完成后，重置进度显示
        if (ui->progressBar) {
            ui->progressBar->setValue(0);
        }
        if (ui->label_currentFile) {
            ui->label_currentFile->setText("无文件发送");
        }
        if (ui->label_speed) {
            ui->label_speed->setText("0.00 MB/s");
        }
        delete file;
        socket->deleteLater();
        m_isSendingFile = false; // 传输结束
        startFileTransfer(); // 尝试发送下一个文件
    });

    // 修正：确保客户端连接的端口与服务器监听的端口一致
    socket->connectToHost(ipAddress, port);
}

// 接收日志消息的槽函数
void MainWindow::onLogMessage(const QString &message)
{
    if (ui->textEdit_Log) {
        ui->textEdit_Log->append(message);
    }
}

// 更新统计标签的槽函数
void MainWindow::updateStatistics()
{
    int totalFiles = m_fileStatus.size();
    int successFiles = 0;
    int failedFiles = 0;

    for (FileStatus status : m_fileStatus.values()) {
        if (status == Success) {
            successFiles++;
        } else if (status == Failure) {
            failedFiles++;
        }
    }

    if (ui->label_total) {
        ui->label_total->setText(QString("总文件数：%1").arg(totalFiles));
    }
    if (ui->label_success) {
        ui->label_success->setText(QString("成功发送：%1").arg(successFiles));
    }
    if (ui->label_failed) {
        ui->label_failed->setText(QString("发送失败：%1").arg(failedFiles));
    }
}
