#ifndef FILESENDERWORKER_H
#define FILESENDERWORKER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QElapsedTimer>
#include <QTimer>

class FileSenderWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileSenderWorker(QObject *parent = nullptr);
    ~FileSenderWorker();

    // 允许MainWindow查询和修改worker的状态
    bool isSending() const { return m_isSending; }

public slots:
    void process(const QString& filePath);
    void setSendingStatus(bool isSending) { m_isSending = isSending; }

signals:
    void progress(int value);
    void updateSpeed(double speed);
    void fileSentSuccess(const QString& filePath);
    void fileSentFailure(const QString& filePath, const QString& error);
    void finished();
    void taskStarted(const QString& filePath);

private slots:
    void onConnected();
    void onDisconnected();
    void onBytesWritten(qint64 bytes);
    void onReadyRead();
    void onSocketError(QTcpSocket::SocketError socketError);
    void onTimeout();

private:
    void sendFileMetadata();
    void sendNextChunk();
    void closeConnectionAndFinish(const QString& errorMessage = QString());

    QTcpSocket *myTcpSocket;
    QFile *myFile; // 注意：这是一个 QObject 的子对象，无需手动 delete
    QString m_filePath;
    qint64 m_fileSize;
    qint64 m_totalSent;
    bool m_isSending;

    QElapsedTimer m_speedTimer;
    qint64 m_totalBytesSentInPeriod;

    QTimer *responseTimer;
};

#endif // FILESENDERWORKER_H
