#include "filesenderworker.h"
#include <QDebug>
#include <QFileInfo>
#include <QHostAddress>

FileSenderWorker::FileSenderWorker(QObject *parent)
    : QObject(parent)
    , myTcpSocket(new QTcpSocket(this))
    , myFile(nullptr)
    , m_isSending(false)
    , m_totalSent(0)
    , m_fileSize(0)
    , m_totalBytesSentInPeriod(0)
    , responseTimer(new QTimer(this))
{
    // Connect persistent signals in the constructor to avoid duplicates
    connect(myTcpSocket, &QTcpSocket::disconnected, this, &FileSenderWorker::onDisconnected);
    connect(myTcpSocket, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &FileSenderWorker::onSocketError);
    connect(myTcpSocket, &QTcpSocket::connected, this, &FileSenderWorker::onConnected);

    connect(responseTimer, &QTimer::timeout, this, &FileSenderWorker::onTimeout);
    responseTimer->setSingleShot(true);
}

FileSenderWorker::~FileSenderWorker()
{
    // myFile is a child of FileSenderWorker, so it's automatically deleted by QObject
    // We only need to ensure the socket is disconnected
    if (myTcpSocket->state() != QAbstractSocket::UnconnectedState) {
        myTcpSocket->disconnectFromHost();
        myTcpSocket->waitForDisconnected();
    }
}

void FileSenderWorker::process(const QString& filePath)
{
    // If the worker is already busy, do nothing
    if (m_isSending) {
        qWarning() << "Worker is already busy with a task.";
        return;
    }

    m_filePath = filePath;
    m_isSending = true;
    m_totalSent = 0;
    m_totalBytesSentInPeriod = 0;

    emit taskStarted(m_filePath);

    myFile = new QFile(m_filePath, this);
    if (!myFile->open(QIODevice::ReadOnly)) {
        closeConnectionAndFinish("Failed to open file.");
        return;
    }

    m_fileSize = myFile->size();
    myTcpSocket->connectToHost(QHostAddress::LocalHost, 65432);
}

void FileSenderWorker::onConnected()
{
    qDebug() << "Successfully connected to server.";
    sendFileMetadata();
    m_speedTimer.start();
    // Connect bytesWritten signal only after connection is established
    connect(myTcpSocket, &QTcpSocket::bytesWritten, this, &FileSenderWorker::onBytesWritten);
}

void FileSenderWorker::onBytesWritten(qint64 bytes)
{
    m_totalSent += bytes;
    m_totalBytesSentInPeriod += bytes;

    if (m_fileSize > 0) {
        int percentage = (m_totalSent * 100) / m_fileSize;
        emit progress(percentage);
    }
    if (m_speedTimer.elapsed() >= 500) {
        double speed = (double)m_totalBytesSentInPeriod / (m_speedTimer.elapsed() / 1000.0) / (1024 * 1024);
        emit updateSpeed(speed);
        m_speedTimer.restart();
        m_totalBytesSentInPeriod = 0;
    }

    sendNextChunk();
}

void FileSenderWorker::onReadyRead()
{
    responseTimer->stop();
    QByteArray response = myTcpSocket->readAll();

    if (response == "SUCCESS") {
        qDebug() << "File sent successfully and received server confirmation.";
        emit fileSentSuccess(m_filePath);
    } else {
        qDebug() << "File sent but received invalid or no confirmation. Response: " << response;
        emit fileSentFailure(m_filePath, "Received invalid or no response from server.");
    }
    closeConnectionAndFinish();
}

void FileSenderWorker::onDisconnected()
{
    if (m_isSending) {
        if (m_totalSent < m_fileSize) {
            closeConnectionAndFinish("Connection lost during transfer.");
        } else if (responseTimer->isActive()) {
            closeConnectionAndFinish("Connection lost while waiting for server response.");
        }
    }
}

void FileSenderWorker::onSocketError(QTcpSocket::SocketError socketError)
{
    Q_UNUSED(socketError); // Suppress unused variable warning
    if (m_isSending) {
        qDebug() << "Socket error occurred: " << myTcpSocket->errorString();
        closeConnectionAndFinish(myTcpSocket->errorString());
    }
}

void FileSenderWorker::onTimeout()
{
    qDebug() << "Timeout waiting for server response for file: " << m_filePath;
    closeConnectionAndFinish("Timeout waiting for server response.");
}

void FileSenderWorker::sendFileMetadata()
{
    QByteArray fileNameBytes = QFileInfo(m_filePath).fileName().toUtf8();
    qint32 fileNameLength = fileNameBytes.size();
    myTcpSocket->write(reinterpret_cast<const char*>(&fileNameLength), sizeof(fileNameLength));
    myTcpSocket->write(fileNameBytes);

    QString sizeStr = QString::number(m_fileSize).leftJustified(16, ' ');
    myTcpSocket->write(sizeStr.toUtf8());

    sendNextChunk();
}

void FileSenderWorker::sendNextChunk()
{
    if (!myFile || !myFile->isOpen()) {
        closeConnectionAndFinish("File not open.");
        return;
    }

    if (m_totalSent < m_fileSize) {
        QByteArray buffer = myFile->read(16 * 1024);
        myTcpSocket->write(buffer);
    } else {
        myFile->close();

        disconnect(myTcpSocket, &QTcpSocket::bytesWritten, this, &FileSenderWorker::onBytesWritten);
        connect(myTcpSocket, &QTcpSocket::readyRead, this, &FileSenderWorker::onReadyRead);
        emit progress(100);

        responseTimer->start(10000); // 10s timeout
    }
}

void FileSenderWorker::closeConnectionAndFinish(const QString& errorMessage)
{
    // myFile is a child object and will be automatically cleaned up.
    // We use deleteLater to safely schedule deletion.
    if (myFile) {
        if (myFile->isOpen()) {
            myFile->close();
        }
        myFile->deleteLater();
        myFile = nullptr; // Reset the pointer to avoid dangling references
    }

    if (!errorMessage.isEmpty()) {
        emit fileSentFailure(m_filePath, errorMessage);
    }

    if (responseTimer->isActive()) {
        responseTimer->stop();
    }

    myTcpSocket->disconnectFromHost();
    m_isSending = false;
    emit finished();
}
