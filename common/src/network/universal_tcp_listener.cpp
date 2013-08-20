#include "universal_tcp_listener.h"
#include "utils/network/tcp_connection_priv.h"
#include <QUrl>
#include "universal_request_processor.h"
#include "proxy_sender_connection_processor.h"

// -------------------------------- QnUniversalListener ---------------------------------

QnUniversalTcpListener::QnUniversalTcpListener(const QHostAddress& address, int port, int maxConnections):
    QnTcpListener(address, port, maxConnections),
    m_proxyPoolSize(0)
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

void QnUniversalTcpListener::setProxyReceiverUrl(const QUrl& url)
{
    m_backConnectUrl = url;
}

void QnUniversalTcpListener::addProxySenderConnections(int size)
{
    if (m_needStop)
        return;

    for (int i = 0; i < size; ++i) {
        QnProxySenderConnection* connect = new QnProxySenderConnection(m_backConnectUrl, this);
        connect->start();
        addOwnership(connect);
    }
}

TCPSocket* QnUniversalTcpListener::getProxySocket(const QUrl& url, int timeout)
{
    QMutexLocker lock(&m_proxyMutex);
    QMultiMap<QUrl, TCPSocket*>::iterator itr = m_awaitingProxyConnections.find(url);
    while (itr == m_awaitingProxyConnections.end() && m_proxyConExists.contains(url)) {
        if (!m_proxyWaitCond.wait(&m_proxyMutex, timeout))
            break;
        itr = m_awaitingProxyConnections.find(url);
    }

    if (itr == m_awaitingProxyConnections.end())
        return 0;

    m_awaitingProxyConnections.erase(itr);
    return itr.value();
}

void QnUniversalTcpListener::setProxyPoolSize(int value)
{
    m_proxyPoolSize = value;
}

bool QnUniversalTcpListener::registerProxyReceiverConnection(const QUrl& url, TCPSocket* socket)
{
    QMutexLocker lock(&m_proxyMutex);
    if (m_awaitingProxyConnections.size() < m_proxyPoolSize) {
        m_awaitingProxyConnections.insert(url, socket);
        m_proxyConExists << url;
        return true;
    }
    return false;
}
