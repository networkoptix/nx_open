#include "reverse_connection_manager.h"

#include <nx/utils/log/log.h>

namespace {

static const int kProxyKeepAliveIntervalMs = 40 * 1000;
static const int kProxyConnectionsToRequest = 3;

} // namespace

namespace nx {
namespace mediaserver {

QSharedPointer<nx::network::AbstractStreamSocket> ReverseConnectionManager::getProxySocket(
    const QString& guid, int timeoutMs, const SocketRequest& socketRequest)
{
    NX_LOG(lit("QnHttpConnectionListener: reverse connection from %1 is needed")
        .arg(guid), cl_logDEBUG1);

    QnMutexLocker lock(&m_proxyMutex);
    auto& serverPool = m_proxyPool[guid]; //< Get or create with new code.
    if (serverPool.available.isEmpty())
    {
        nx::utils::ElapsedTimer timer;
        timer.restart();
        while (serverPool.available.isEmpty())
        {
            const auto elapsedMs = timer.elapsedMs();
            if (elapsedMs >= timeoutMs)
            {
                NX_LOG(lit(
                    "QnHttpConnectionListener: reverse connection from %1 was waited too long (%2 ms)")
                    .arg(guid).arg(elapsedMs), cl_logERROR);
                return QSharedPointer<nx::network::AbstractStreamSocket>();
            }

            if (serverPool.requested == 0 || //< Was not requested by another thread.
                serverPool.timer.elapsed() > timeoutMs) // Or request was not filed within timeout.
            {
                serverPool.requested = kProxyConnectionsToRequest;
                serverPool.timer.restart();
                socketRequest((int)serverPool.requested);
            }

            m_proxyCondition.wait(&m_proxyMutex, timeoutMs - elapsedMs);
        }
    }

    auto socket = serverPool.available.front().socket;
    serverPool.available.pop_front();
    NX_LOG(lit("QnHttpConnectionListener: reverse connection from %1 is used, %2 more avaliable")
        .arg(guid).arg(serverPool.available.size()), cl_logDEBUG1);

    socket->setNonBlockingMode(false);
    return socket;
}

bool ReverseConnectionManager::registerProxyReceiverConnection(
    const QString& guid, QSharedPointer<nx::network::AbstractStreamSocket> socket)
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

void ReverseConnectionManager::doPeriodicTasks()
{
    QnMutexLocker lock(&m_proxyMutex);
    for (auto& serverPool : m_proxyPool)
    {
        while (!serverPool.available.isEmpty()
            && serverPool.available.front().timer.elapsedMs() > kProxyKeepAliveIntervalMs)
        {
            serverPool.available.pop_front();
        }
    }
}

} // namespace mediaserver
} // namespace nx
