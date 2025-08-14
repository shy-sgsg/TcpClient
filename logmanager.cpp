#include "logmanager.h"
#include <QDebug>
#include <QMutex>

// 原始消息处理函数指针
static QtMessageHandler originalMessageHandler = nullptr;

LogManager& LogManager::instance()
{
    static LogManager logManager;
    return logManager;
}

void LogManager::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 使用线程本地变量来保护，避免同一个线程内的递归调用
    static thread_local bool isInsideHandler = false;
    if (isInsideHandler) {
        if (originalMessageHandler) {
            originalMessageHandler(type, context, msg);
        }
        return;
    }

    isInsideHandler = true;

    // 将消息转发给主窗口，不添加任何额外的前缀
    LogManager::instance().logMessage(msg);

    // 调用原始的消息处理函数，将消息打印到终端
    if (originalMessageHandler) {
        originalMessageHandler(type, context, msg);
    }

    isInsideHandler = false;
}

LogManager::LogManager(QObject *parent)
    : QObject(parent)
{
    // 保存原始的消息处理函数，然后安装我们自己的
    originalMessageHandler = qInstallMessageHandler(messageHandler);
}

