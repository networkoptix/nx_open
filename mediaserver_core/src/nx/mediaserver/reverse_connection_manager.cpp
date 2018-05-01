#include "reverse_connection_manager.h"

#include <nx/utils/log/log.h>
#include <transaction/transaction.h>
#include <common/common_module.h>
#include <transaction/message_bus_adapter.h>

namespace {

static const int kProxyKeepAliveIntervalMs = 40 * 1000;
static const int kProxyConnectionsToRequest = 3;

} // namespace

namespace nx {
namespace mediaserver {

ReverseConnectionManager::ReverseConnectionManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

std::unique_ptr<nx::network::AbstractStreamSocket> ReverseConnectionManager::getProxySocket(
    const QnUuid& guid, std::chrono::milliseconds timeout)
{
    NX_DEBUG(this, lit("QnHttpConnectionListener: reverse connection from %1 is needed")
        .arg(guid.toString()));
    
    auto socketRequest = [&](int socketCount)
    {
        ec2::QnTransaction<ec2::ApiReverseConnectionData> tran(
            ec2::ApiCommand::openReverseConnection,
            commonModule()->moduleGUID());
        tran.params.targetServer = commonModule()->moduleGUID();
        tran.params.socketCount = socketCount;
        commonModule()->ec2Connection()->messageBus()->sendTransaction(tran, guid);
    };

    QnMutexLocker lock(&m_proxyMutex);
    auto& serverPool = m_proxyPool[guid.toString()]; //< Get or create with new code.
    if (serverPool.available.empty())
    {
        nx::utils::ElapsedTimer timer;
        timer.restart();
        while (serverPool.available.empty())
        {
            const auto elapsed = timer.elapsed();
            if (elapsed >= timeout)
            {
                NX_ERROR(this, lit(
                    "QnHttpConnectionListener: reverse connection from %1 was waited too long (%2 ms)")
                    .arg(guid.toString())
                    .arg(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
                return std::unique_ptr<nx::network::AbstractStreamSocket>();
            }

            if (serverPool.requested == 0 || //< Was not requested by another thread.
                serverPool.timer.elapsed() > timeout) // Or request was not filed within timeout.
            {
                serverPool.requested = kProxyConnectionsToRequest;
                serverPool.timer.restart();
                socketRequest((int)serverPool.requested);
            }

            auto timeoutMs = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            m_proxyCondition.wait(&m_proxyMutex, timeoutMs - elapsedMs);
        }
    }
    auto socket = std::move(serverPool.available.front().socket);
    serverPool.available.pop_front();
    NX_DEBUG(this, lit("QnHttpConnectionListener: reverse connection from %1 is used, %2 more avaliable")
        .arg(guid.toString()).arg(serverPool.available.size()));
    socket->setNonBlockingMode(false);
    return socket;
}

bool ReverseConnectionManager::registerProxyReceiverConnection(
    const QString& guid, std::unique_ptr<nx::network::AbstractStreamSocket> socket)
{
    QnMutexLocker lock(&m_proxyMutex);
    auto serverPool = m_proxyPool.find(guid);
    if (serverPool == m_proxyPool.end() || serverPool->second.requested == 0)
    {
        NX_WARNING(this, lit("QnHttpConnectionListener: reverse connection was not requested from %1")
            .arg(guid));
        return false;
    }

    socket->setNonBlockingMode(true);
    serverPool->second.requested -= 1;
    serverPool->second.available.emplace_back(AwaitProxyInfo(std::move(socket)));
    NX_DEBUG(this, lit(
        "QnHttpConnectionListener: got new reverse connection from %1, there is (are) %2 avaliable and %3 requested")
        .arg(guid).arg(serverPool->second.available.size()).arg(serverPool->second.requested));

    m_proxyCondition.wakeAll();
    return true;
}

void ReverseConnectionManager::doPeriodicTasks()
{
    QnMutexLocker lock(&m_proxyMutex);
    for (auto& serverPool : m_proxyPool)
    {
        while (!serverPool.second.available.empty()
            && serverPool.second.available.front().timer.elapsedMs() > kProxyKeepAliveIntervalMs)
        {
            serverPool.second.available.pop_front();
        }
    }
}

} // namespace mediaserver
} // namespace nx
