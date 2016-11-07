#include "flir_websocket_proxy.h"
#include "flir_io_executor.h"

FlirWebSocketProxy::FlirWebSocketProxy()
{
    auto executorThread = FlirIOExecutor::instance()->getThread();
    executorThread->start();

    m_socket = new QWebSocket();
    m_socket->setParent(this);
    moveToThread(executorThread);
}

QWebSocket* FlirWebSocketProxy::getSocket()
{
    return m_socket;
}

void FlirWebSocketProxy::open(const QUrl& url)
{
    QMetaObject::invokeMethod(
        this,
        "openInternal",
        Qt::QueuedConnection,
        Q_ARG(QUrl, url));
};

void FlirWebSocketProxy::sendTextMessage(const QString& message)
{
    QMetaObject::invokeMethod(
        this,
        "sendTextMessageInternal",
        Qt::QueuedConnection,
        Q_ARG(const QString&, message));
};

void FlirWebSocketProxy::openInternal(const QUrl& url)
{
    m_socket->open(url);
}

void FlirWebSocketProxy::sendTextMessageInternal(const QString& message)
{
    m_socket->sendTextMessage(message);
}
