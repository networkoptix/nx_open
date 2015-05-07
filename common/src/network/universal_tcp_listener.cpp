
#include "universal_tcp_listener.h"

#include "utils/network/tcp_connection_priv.h"
#include "utils/common/log.h"
#include <QtCore/QUrl>
#include "universal_request_processor.h"
#include "proxy_sender_connection_processor.h"
#include "utils/network/socket.h"


static const int PROXY_KEEP_ALIVE_INTERVAL = 40 * 1000;
static const int PROXY_CONNECTIONS_TO_REQUEST = 5;

// -------------------------------- QnUniversalListener ---------------------------------

QnUniversalTcpListener::QnUniversalTcpListener(
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl )
:
    QnTcpListener( address, port, maxConnections, useSsl ),
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
}

QnUniversalTcpListener::InstanceFunc QnUniversalTcpListener::findHandler(
        const QByteArray& protocol, const nx_http::Request& request)
{
    if (isProxy(request))
        return m_proxyInfo.proxyHandler;

    QString normPath = request.requestLine.url.path();
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
        return m_handlers[bestIdx].instanceFunc;

    // check default '*' path handler
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == protocol && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc;
    }

    // check default '*' path and protocol handler
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == "*" && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc;
    }
    return 0;
}

QnTCPConnectionProcessor* QnUniversalTcpListener::createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket)
{
    return new QnUniversalRequestProcessor(clientSocket, this, m_needAuth);
}

void QnUniversalTcpListener::setProxySelfId(const QString& selfId)
{
    m_selfIdForProxy = selfId;
}

void QnUniversalTcpListener::addProxySenderConnections(const SocketAddress& proxyUrl, int size)
{
    if (m_needStop)
        return;

    for (int i = 0; i < size; ++i) {
        auto connect = new QnProxySenderConnection(proxyUrl, m_selfIdForProxy, this);
        connect->start();
        addOwnership(connect);
    }
}

QSharedPointer<AbstractStreamSocket> QnUniversalTcpListener::getProxySocket(
        const QString& guid, int timeout, const SocketRequest& socketRequest)
{
    QMutexLocker lock(&m_proxyMutex);
    auto& serverPool = m_proxyPool[guid]; // get or create with new code
    if (serverPool.isEmpty())
    {
        if (socketRequest)
            socketRequest(PROXY_CONNECTIONS_TO_REQUEST);

        while (serverPool.isEmpty())
            if (!m_proxyCondition.wait(&m_proxyMutex, timeout))
                return QSharedPointer<AbstractStreamSocket>();
    }

    auto socket = serverPool.front().socket;
    socket->setNonBlockingMode(false);
    serverPool.pop_front();
    return socket;
}

bool QnUniversalTcpListener::registerProxyReceiverConnection(
        const QString& guid, QSharedPointer<AbstractStreamSocket> socket)
{
    QMutexLocker lock(&m_proxyMutex);
    auto serverPool = m_proxyPool.find(guid);
    if (serverPool == m_proxyPool.end()) {
        NX_LOG(lit("QnUniversalTcpListener: proxy was not requested from %2")
               .arg(guid), cl_logWARNING);
        return false;
    }

    if (serverPool->size() > PROXY_CONNECTIONS_TO_REQUEST * 2) {
        NX_LOG(lit("QnUniversalTcpListener: too many proxies from %2")
               .arg(guid), cl_logINFO);
        return false;
    }

    socket->setNonBlockingMode(true);
    serverPool->push_back(AwaitProxyInfo(socket));
    m_proxyCondition.wakeAll();
    return true;
}

void QnUniversalTcpListener::doPeriodicTasks()
{
    QnTcpListener::doPeriodicTasks();

    QMutexLocker lock(&m_proxyMutex);
    for (auto& serverPool : m_proxyPool)
        while(!serverPool.isEmpty() &&
              serverPool.front().timer.elapsed() > PROXY_KEEP_ALIVE_INTERVAL)
        {
            serverPool.front().socket->send(QByteArray("HTTP 200 OK\r\n\r\n"));
            serverPool.pop_front();
        }
}

void QnUniversalTcpListener::disableAuth()
{
    m_needAuth = false;
}
