#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QMessageLogContext>

class LogManager : public QObject
{
    Q_OBJECT

public:
    static LogManager& instance();
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

signals:
    void logMessage(const QString &message);

private:
    explicit LogManager(QObject *parent = nullptr);
    Q_DISABLE_COPY(LogManager)
};

#endif // LOGMANAGER_H
