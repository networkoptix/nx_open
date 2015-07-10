
#include "universal_tcp_listener.h"

#include "utils/network/tcp_connection_priv.h"
#include "utils/common/log.h"
#include <QtCore/QUrl>
#include "universal_request_processor.h"
#include "proxy_sender_connection_processor.h"
#include "utils/network/socket.h"
#include "common/common_module.h"


static const int PROXY_KEEP_ALIVE_INTERVAL = 40 * 1000;
static const int PROXY_CONNECTIONS_TO_REQUEST = 3;

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

void QnUniversalTcpListener::addProxySenderConnections(const SocketAddress& proxyUrl, int size)
{
    if (m_needStop)
        return;

    NX_LOG(lit("QnUniversalTcpListener: %1 reverse connection(s) to %2 is(are) needed")
            .arg(size).arg(proxyUrl.toString()), cl_logDEBUG1);

    for (int i = 0; i < size; ++i) {
        auto connect = new QnProxySenderConnection(proxyUrl, qnCommon->moduleGUID(), this);
        connect->start();
        addOwnership(connect);
    }
}

QSharedPointer<AbstractStreamSocket> QnUniversalTcpListener::getProxySocket(
        const QString& guid, int timeout, const SocketRequest& socketRequest)
{
    NX_LOG(lit("QnUniversalTcpListener: reverse connection from %1 is needed")
            .arg(guid), cl_logDEBUG1);

    QMutexLocker lock(&m_proxyMutex);
    auto& serverPool = m_proxyPool[guid]; // get or create with new code
    if (serverPool.available.isEmpty())
    {
        QElapsedTimer timer;
        timer.start();
        while (serverPool.available.isEmpty())
        {
            const auto elapsed = timer.elapsed();
            if (elapsed >= timeout)
            {
                NX_LOG(lit("QnUniversalTcpListener: reverse connection from %1 was waited too long (%2 ms)")
                        .arg(guid).arg(elapsed), cl_logERROR);
                return QSharedPointer<AbstractStreamSocket>();
            }

            if (serverPool.requested == 0)
                socketRequest(serverPool.requested = PROXY_CONNECTIONS_TO_REQUEST);

            m_proxyCondition.wait(&m_proxyMutex, timeout - elapsed);
        }
    }

    auto socket = serverPool.available.front().socket;
    serverPool.available.pop_front();
    NX_LOG(lit("QnUniversalTcpListener: reverse connection from %1 is used, %2 more avaliable")
            .arg(guid).arg(serverPool.available.size()), cl_logDEBUG1);

    socket->setNonBlockingMode(false);
    return socket;
}

bool QnUniversalTcpListener::registerProxyReceiverConnection(
        const QString& guid, QSharedPointer<AbstractStreamSocket> socket)
{
    QMutexLocker lock(&m_proxyMutex);
    auto serverPool = m_proxyPool.find(guid);
    if (serverPool == m_proxyPool.end() || serverPool->requested == 0) {
        NX_LOG(lit("QnUniversalTcpListener: reverse connection was not requested from %1")
               .arg(guid), cl_logWARNING);
        return false;
    }

    socket->setNonBlockingMode(true);
    serverPool->requested -= 1;
    serverPool->available.push_back(AwaitProxyInfo(socket));
    NX_LOG(lit("QnUniversalTcpListener: got new reverse connection from %1, there is(are) %2 avaliable and %3 requested")
           .arg(guid).arg(serverPool->available.size()).arg(serverPool->requested), cl_logDEBUG1);

    m_proxyCondition.wakeAll();
    return true;
}

void QnUniversalTcpListener::doPeriodicTasks()
{
    QnTcpListener::doPeriodicTasks();

    QMutexLocker lock(&m_proxyMutex);
    for (auto& serverPool : m_proxyPool)
        while(!serverPool.available.isEmpty() &&
              serverPool.available.front().timer.elapsed() > PROXY_KEEP_ALIVE_INTERVAL)
            serverPool.available.pop_front();
}

void QnUniversalTcpListener::disableAuth()
{
    m_needAuth = false;
}
