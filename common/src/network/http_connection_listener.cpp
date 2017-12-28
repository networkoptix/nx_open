#include "http_connection_listener.h"

#include <QtCore/QUrl>

#include <nx/utils/log/log.h>
#include <nx/network/socket.h>
#include <network/tcp_connection_priv.h>
#include <common/common_module.h>

static const int kProxyKeepAliveInterval = 40 * 1000;
static const int kProxyConnectionsToRequest = 3;

QnHttpConnectionListener::QnHttpConnectionListener(
    QnCommonModule* commonModule,
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl)
    :
    QnTcpListener(commonModule, address, port, maxConnections, useSsl),
    m_needAuth(true)
{
}

QnHttpConnectionListener::~QnHttpConnectionListener()
{
    stop();
}

bool QnHttpConnectionListener::isProxy(const nx::network::http::Request& request)
{
    return m_proxyInfo.proxyHandler && m_proxyInfo.proxyCond(commonModule(), request);
}

bool QnHttpConnectionListener::needAuth() const
{
    return m_needAuth;
}

QnHttpConnectionListener::InstanceFunc QnHttpConnectionListener::findHandler(
    const QByteArray& protocol, const nx::network::http::Request& request)
{
    if (isProxy(request))
        return m_proxyInfo.proxyHandler;

    QString normPath = normalizedPath(request.requestLine.url.path());

    int bestPathLen = -1;
    int bestIdx = -1;
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        HandlerInfo h = m_handlers[i];
        if ((m_handlers[i].protocol == "*" || m_handlers[i].protocol == protocol)
            && normPath.startsWith(m_handlers[i].path))
        {
            int pathLen = m_handlers[i].path.length();
            if (pathLen > bestPathLen)
            {
                bestIdx = i;
                bestPathLen = pathLen;
            }
        }
    }
    if (bestIdx >= 0)
        return m_handlers[bestIdx].instanceFunc;

    // Check default "*" path handler.
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == protocol && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc;
    }

    // Check default "*' path and protocol handler.
    for (int i = 0; i < m_handlers.size(); ++i)
    {
        if (m_handlers[i].protocol == "*" && m_handlers[i].path == QLatin1String("*"))
            return m_handlers[i].instanceFunc;
    }

    return 0;
}

QSharedPointer<AbstractStreamSocket> QnHttpConnectionListener::getProxySocket(
    const QString& guid, int timeout, const SocketRequest& socketRequest)
{
    NX_LOG(lit("QnHttpConnectionListener: reverse connection from %1 is needed")
        .arg(guid), cl_logDEBUG1);

    QnMutexLocker lock(&m_proxyMutex);
    auto& serverPool = m_proxyPool[guid]; //< Get or create with new code.
    if (serverPool.available.isEmpty())
    {
        QElapsedTimer timer;
        timer.start();
        while (serverPool.available.isEmpty())
        {
            const auto elapsed = timer.elapsed();
            if (elapsed >= timeout)
            {
                NX_LOG(lit(
                    "QnHttpConnectionListener: reverse connection from %1 was waited too long (%2 ms)")
                    .arg(guid).arg(elapsed), cl_logERROR);
                return QSharedPointer<AbstractStreamSocket>();
            }

            if (serverPool.requested == 0 || //< Was not requested by another thread.
                serverPool.timer.elapsed() > timeout) // Or request was not filed within timeout.
            {
                serverPool.requested = kProxyConnectionsToRequest;
                serverPool.timer.restart();
                socketRequest((int) serverPool.requested);
            }

            m_proxyCondition.wait(&m_proxyMutex, timeout - elapsed);
        }
    }

    auto socket = serverPool.available.front().socket;
    serverPool.available.pop_front();
    NX_LOG(lit("QnHttpConnectionListener: reverse connection from %1 is used, %2 more avaliable")
        .arg(guid).arg(serverPool.available.size()), cl_logDEBUG1);

    socket->setNonBlockingMode(false);
    return socket;
}

bool QnHttpConnectionListener::registerProxyReceiverConnection(
    const QString& guid, QSharedPointer<AbstractStreamSocket> socket)
{
    QnMutexLocker lock(&m_proxyMutex);
    auto serverPool = m_proxyPool.find(guid);
    if (serverPool == m_proxyPool.end() || serverPool->requested == 0)
    {
        NX_LOG(lit("QnHttpConnectionListener: reverse connection was not requested from %1")
           .arg(guid), cl_logWARNING);
        return false;
    }

    socket->setNonBlockingMode(true);
    serverPool->requested -= 1;
    serverPool->available.push_back(AwaitProxyInfo(socket));
    NX_LOG(lit(
        "QnHttpConnectionListener: got new reverse connection from %1, there is (are) %2 avaliable and %3 requested")
        .arg(guid).arg(serverPool->available.size()).arg(serverPool->requested), cl_logDEBUG1);

    m_proxyCondition.wakeAll();
    return true;
}

void QnHttpConnectionListener::doPeriodicTasks()
{
    QnTcpListener::doPeriodicTasks();

    QnMutexLocker lock(&m_proxyMutex);
    for (auto& serverPool: m_proxyPool)
    {
        while(!serverPool.available.isEmpty()
            && serverPool.available.front().timer.elapsed() > kProxyKeepAliveInterval)
        {
            serverPool.available.pop_front();
        }
    }
}

void QnHttpConnectionListener::disableAuth()
{
    m_needAuth = false;
}

void QnHttpConnectionListener::doAddHandler(
    const QByteArray& protocol, const QString& path, InstanceFunc instanceFunc)
{
    HandlerInfo handler;
    handler.protocol = protocol;
    handler.path = path;
    handler.instanceFunc = instanceFunc;
    m_handlers.append(handler);
}
