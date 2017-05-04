#include "flir_web_socket_proxy.h"
#include "flir_io_executor.h"

namespace nx {
namespace plugins {
namespace flir {
namespace nexus {

WebSocketProxy::WebSocketProxy(QObject* parent): QObject(parent)
{
    auto executorThread = IoExecutor::instance()->getThread();
    executorThread->start();

    m_socket = new QWebSocket();
    m_socket->setParent(this);
    moveToThread(executorThread);
}

QWebSocket* WebSocketProxy::getSocket()
{
    return m_socket;
}

void WebSocketProxy::open(const QUrl& url)
{
    QMetaObject::invokeMethod(
        this,
        "openInternal",
        Qt::QueuedConnection,
        Q_ARG(QUrl, url));
};

void WebSocketProxy::sendTextMessage(const QString& message)
{
    QMetaObject::invokeMethod(
        this,
        "sendTextMessageInternal",
        Qt::QueuedConnection,
        Q_ARG(const QString&, message));
};

void WebSocketProxy::openInternal(const QUrl& url)
{
    m_socket->open(url);
}

void WebSocketProxy::sendTextMessageInternal(const QString& message)
{
    m_socket->sendTextMessage(message);
}

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx