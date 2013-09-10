#include "universal_tcp_listener.h"
#include "utils/network/tcp_connection_priv.h"
#include <QtCore/QUrl>
#include "universal_request_processor.h"
#include "proxy_sender_connection_processor.h"
#include "utils/network/socket.h"


static const int PROXY_KEEP_ALIVE_INTERVAL = 40 * 1000;

// -------------------------------- QnUniversalListener ---------------------------------

QnUniversalTcpListener::QnUniversalTcpListener(const QHostAddress& address, int port, int maxConnections):
    QnTcpListener(address, port, maxConnections),
    m_proxyPoolSize(0)
{

}

QnUniversalTcpListener::~QnUniversalTcpListener()
{
    stop();
    for (ProxyList::Iterator itr = m_awaitingProxyConnections.begin(); itr != m_awaitingProxyConnections.end(); ++itr)
        delete itr.value().socket;
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createNativeProcessor(AbstractStreamSocket* clientSocket, const QByteArray& protocol, const QString& path)
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

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(AbstractStreamSocket* clientSocket, QnTcpListener* owner)
{
    return new QnUniversalRequestProcessor(clientSocket, owner);
}

void QnUniversalTcpListener::setProxyParams(const QUrl& proxyServerUrl, const QString& selfId)
{
    m_proxyServerUrl = proxyServerUrl;
    m_selfIdForProxy = selfId;
}

void QnUniversalTcpListener::addProxySenderConnections(int size)
{
    if (m_needStop)
        return;

    for (int i = 0; i < size; ++i) {
        QnProxySenderConnection* connect = new QnProxySenderConnection(m_proxyServerUrl, m_selfIdForProxy, this);
        connect->start();
        addOwnership(connect);
    }
}

AbstractStreamSocket* QnUniversalTcpListener::getProxySocket(const QString& guid, int timeout)
{
    QMutexLocker lock(&m_proxyMutex);
    ProxyList::iterator itr = m_awaitingProxyConnections.find(guid);
    while (itr == m_awaitingProxyConnections.end() && m_proxyConExists.contains(guid)) {
        if (!m_proxyWaitCond.wait(&m_proxyMutex, timeout))
            break;
        itr = m_awaitingProxyConnections.find(guid);
    }

    if (itr == m_awaitingProxyConnections.end())
        return 0;
    AbstractStreamSocket* result = itr.value().socket;
    result->setNonBlockingMode(false);
    m_awaitingProxyConnections.erase(itr);
    return result;
}

void QnUniversalTcpListener::setProxyPoolSize(int value)
{
    m_proxyPoolSize = value;
}

bool QnUniversalTcpListener::registerProxyReceiverConnection(const QString& guid, AbstractStreamSocket* socket)
{
    QMutexLocker lock(&m_proxyMutex);
    if (m_awaitingProxyConnections.size() < m_proxyPoolSize * 2) {
        m_awaitingProxyConnections.insert(guid, AwaitProxyInfo(socket));
        socket->setNonBlockingMode(true);
        m_proxyConExists << guid;
        m_proxyWaitCond.wakeAll();
        return true;
    }
    return false;
}

void QnUniversalTcpListener::doPeriodicTasks()
{
    QnTcpListener::doPeriodicTasks();

    QMutexLocker lock(&m_proxyMutex);

    for (ProxyList::Iterator itr = m_awaitingProxyConnections.begin(); itr != m_awaitingProxyConnections.end();)
    {
        AwaitProxyInfo& info = itr.value();
        if (info.timer.elapsed() > PROXY_KEEP_ALIVE_INTERVAL)
        {
            info.timer.restart();
            int sended = info.socket->send(QByteArray("PROXY 200 OK\r\n\r\n"));
            if (sended < 1)
            {
                delete info.socket;
                itr = m_awaitingProxyConnections.erase(itr);
                continue;
            }
        }
        ++itr;
    }
}
