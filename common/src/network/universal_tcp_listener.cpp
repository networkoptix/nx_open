#include "universal_tcp_listener.h"
#include "utils/network/tcp_connection_priv.h"
#include <QUrl>
#include "universal_request_processor.h"
#include "back_tcp_connection.h"

// -------------------------------- QnUniversalListener ---------------------------------

QnUniversalTcpListener::QnUniversalTcpListener(const QHostAddress& address, int port, int maxConnections):
    QnTcpListener(address, port, maxConnections)
{

}

QnUniversalTcpListener::~QnUniversalTcpListener()
{
    stop();
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createNativeProcessor(TCPSocket* clientSocket, const QByteArray& protocol, const QString& path)
{
    QString normPath = path.startsWith(L'/') ? path.mid(1) : path;
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        HandlerInfo h = m_handlers[i];
        if (m_handlers[i].protocol == protocol && normPath.startsWith(m_handlers[i].path))
            return m_handlers[i].instanceFunc(clientSocket, this);
    }

    // check default '*' path handler
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == protocol && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc(clientSocket, this);
    }

    // check default '*' path and protocol handler
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == "*" && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc(clientSocket, this);
    }

    return 0;
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnUniversalRequestProcessor(clientSocket, owner);
}

void QnUniversalTcpListener::addBackConnectionPool(const QUrl& url, int size)
{
    m_backConnectUrl = url;
    for (int i = 0; i < size; ++i) {
        QnBackTcpConnection* connect = new QnBackTcpConnection(url, this);
        connect->start();
    }
}

void QnUniversalTcpListener::onBackConnectSpent(QnLongRunnable* processor)
{
    addOwnership(processor);

    QnBackTcpConnection* connect = new QnBackTcpConnection(m_backConnectUrl, this);
    connect->start();
}
