
#include "universal_tcp_listener.h"

#include "utils/network/tcp_connection_priv.h"
#include <QtCore/QUrl>
#include "universal_request_processor.h"
#include "proxy_sender_connection_processor.h"
#include "utils/network/socket.h"


static const int PROXY_KEEP_ALIVE_INTERVAL = 40 * 1000;

// -------------------------------- QnUniversalListener ---------------------------------

QnUniversalTcpListener::QnUniversalTcpListener(
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl )
:
    QnTcpListener( address, port, maxConnections, useSsl ),
    m_proxyPoolSize( 0 ),
    m_needAuth( true )
{
}

QnUniversalTcpListener::~QnUniversalTcpListener()
{
    stop();
}
bool QnUniversalTcpListener::isProxy(const nx_http::Request& request)
{
    return (m_proxyInfo.proxyHandler && m_proxyInfo.proxyCond(request));
};
QnTCPConnectionProcessor* QnUniversalTcpListener::createNativeProcessor(
    QSharedPointer<AbstractStreamSocket> clientSocket,
    const QByteArray& protocol,
    const nx_http::Request& request)
{
    if (isProxy(request))
        return m_proxyInfo.proxyHandler(clientSocket, this);

    QString normPath = request.requestLine.url.path();;
    while (normPath.startsWith(L'/'))
        normPath = normPath.mid(1);

    int bestPathLen = -1;
    int bestIdx = -1;
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        HandlerInfo h = m_handlers[i];
        if ((m_handlers[i].protocol == "*" || m_handlers[i].protocol == protocol) && normPath.startsWith(m_handlers[i].path))
        {
            int pathLen = m_handlers[i].path.length();
            if (pathLen > bestPathLen) {
                bestIdx = i;
                bestPathLen = pathLen;
            }
        }
    }
    if (bestIdx >= 0)
        return m_handlers[bestIdx].instanceFunc(clientSocket, this);

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

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner)
{
    return new QnUniversalRequestProcessor(clientSocket, owner, m_needAuth);
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

QSharedPointer<AbstractStreamSocket> QnUniversalTcpListener::getProxySocket(const QString& guid, int timeout)
{
    QMutexLocker lock(&m_proxyMutex);
    ProxyList::iterator itr = m_awaitingProxyConnections.find(guid);
    while (itr == m_awaitingProxyConnections.end() && m_proxyConExists.contains(guid)) {
        if (!m_proxyWaitCond.wait(&m_proxyMutex, timeout))
            break;
        itr = m_awaitingProxyConnections.find(guid);
    }

    if (itr == m_awaitingProxyConnections.end())
        return QSharedPointer<AbstractStreamSocket>();
    QSharedPointer<AbstractStreamSocket> result = itr.value().socket;
    result->setNonBlockingMode(false);
    m_awaitingProxyConnections.erase(itr);
    return result;
}

void QnUniversalTcpListener::setProxyPoolSize(int value)
{
    m_proxyPoolSize = value;
}

bool QnUniversalTcpListener::registerProxyReceiverConnection(const QString& guid, QSharedPointer<AbstractStreamSocket> socket)
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
        if (!info.socket->isConnected()) {
            itr = m_awaitingProxyConnections.erase(itr);
            continue;
        }
        else if (info.timer.elapsed() > PROXY_KEEP_ALIVE_INTERVAL)
        {
            info.timer.restart();
            int sended = info.socket->send(QByteArray("PROXY 200 OK\r\n\r\n"));
            if (sended < 1)
            {
                itr = m_awaitingProxyConnections.erase(itr);
                continue;
            }
        }
        ++itr;
    }
}

void QnUniversalTcpListener::disableAuth()
{
    m_needAuth = false;
}
