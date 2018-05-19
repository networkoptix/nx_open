#include "reverse_connection_manager.h"

#include <nx/utils/log/log.h>
#include <transaction/transaction.h>
#include <common/common_module.h>
#include <transaction/message_bus_adapter.h>
#include "reverse_connection_listener.h"
#include <nx/utils/scope_guard.h>
#include <core/resource/media_server_resource.h>

namespace {

static const int kProxyKeepAliveIntervalMs = 40 * 1000;
static const int kProxyConnectionsToRequest = 3;
static const std::chrono::seconds kReverseConnectionTimeout(10);
static const QByteArray kReverseConnectionListenerPath("/proxy-reverse");

} // namespace

namespace nx {
namespace vms {
namespace network {

ReverseConnectionManager::ReverseConnectionManager(QnHttpConnectionListener* tcpListener):
    QnCommonModuleAware(tcpListener->commonModule()),
    m_tcpListener(tcpListener)
{
    tcpListener->addHandler<ReverseConnectionListener>("HTTP", kReverseConnectionListenerPath.mid(1), this);

    connect(connection, &ec2::AbstractECConnection::reverseConnectionRequested,
        this, &ReverseConnectionManager::at_reverseConnectionRequested);
}

ReverseConnectionManager::~ReverseConnectionManager()
{
    decltype (m_runningHttpClients) httpClients;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(httpClients, m_runningHttpClients);
    }
    for (const auto& client: httpClients)
        client->pleaseStopSync();
}

void ReverseConnectionManager::at_reverseConnectionRequested(
    const ec2::ApiReverseConnectionData& data)
{
    QnMutexLocker lock(&m_mutex);

    NX_DEBUG(this, lm("QnHttpConnectionListener: %1 reverse connection(s) to %2 is(are) needed")
        .arg(size).arg(proxyUrl.toString()));

    QnRoute route = commonModule()->router()->routeTo(data.targetServer);
    if (!route.gatewayId.isNull() || route.addr.isNull())
    {
        NX_WARNING(this, 
            lm("Got reverse connection request that can't be processed. Target server=%1").arg(data.targetServer));
        return;
    }
    auto server = resourcePool()->getResourceById<QnMediaServerResource>(data.targetServer);
    if (!server)
    {
        NX_WARNING(this, lm("Target server %1 is not known yet").arg(data.targetServer));
        return;
    }

    for (int i = 0; i < data.socketCount; ++i)
    {
        auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
        httpClient->setUserName(server->getId().toByteArray());
        httpClient->setUserPassword(server->getAuthKey().toUtf8());

        httpClient->setSendTimeout(kReverseConnectionTimeout);
        httpClient->setResponseReadTimeout(kReverseConnectionTimeout);

        httpClient->doGet(
            kReverseConnectionListenerPath,
            std::bind(&ReverseConnectionManager::onHttpClientDone, this, httpClient.get()));

        m_runningHttpClients.insert(std::move(httpClient));
    }
}

void ReverseConnectionManager::onHttpClientDone(nx::network::http::AsyncClient* httpClient)
{
    QnMutexLocker lock(&m_mutex);
    if (m_runningHttpClients.find(httpClient) == m_runningHttpClients.end())
        return;

    auto guard = makeScopeGuard(
        [&]
        {
            m_runningHttpClients.erase(httpClient);
        });

    nx::network::http::AsyncClient::State state = httpClient->state();
    if (state == nx::network::http::AsyncClient::State::sFailed)
    {
        NX_WARNING(this,
            lm("Failed to establish reverse connection to the target server=%1").arg(data.targetServer));
        return;
    }
    m_tcpListener->processNewConnection(httpClient->takeSocket());
}

std::unique_ptr<nx::network::AbstractStreamSocket> ReverseConnectionManager::getProxySocket(
    const QnUuid& guid, std::chrono::milliseconds timeout)
{
    NX_DEBUG(this, lit("QnHttpConnectionListener: reverse connection from %1 is needed")
        .arg(guid.toString()));
    
    auto doSocketRequest = [&](int socketCount)
    {
        ec2::QnTransaction<ec2::ApiReverseConnectionData> tran(
            ec2::ApiCommand::openReverseConnection,
            commonModule()->moduleGUID());
        tran.params.targetServer = commonModule()->moduleGUID();
        tran.params.socketCount = socketCount;
        commonModule()->ec2Connection()->messageBus()->sendTransaction(tran, guid);
    };

    QnMutexLocker lock(&m_mutex);
    while (1)
    {
        auto socket = getPreparedSocket(guid);
        if (socket)
            return socket;
        auto& socketList = m_preparedSockets[guid];
        if (socketList.requestedCount == 0)
            doSocketRequest();
        m_proxyCondition.wait(&m_mutex, std::chrono::milliseconds(kReverseConnectionTimeout).count());
    }
#if 0
    auto& socketList = m_preparedSockets[guid];
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
#endif
}

bool ReverseConnectionManager::addIncomingTcpConnection(
    const QString& guid, std::unique_ptr<nx::network::AbstractStreamSocket> socket)
{
    QnMutexLocker lock(&m_mutex);
    auto& preparedSockets = m_preparedSockets[QnUuid(guid)];
    preparedSockets.push_back(std::move(socket));
    
    NX_DEBUG(this, lit(
        "Got new reverse connection from %1, there is (are) %2 avaliable and %3 requested")
        .arg(guid).arg(preparedSockets.size()));

    m_proxyCondition.wakeAll();
    return true;
}

void ReverseConnectionManager::doPeriodicTasks()
{
    QnMutexLocker lock(&m_proxyMutex);
    for (auto& socketPool: m_preparedSockets)
    {
        for (const auto& socket: socketPool)
        {
            sendKeepAliveMessage(socket);
        }
}

std::unique_ptr<nx::network::AbstractStreamSocket> ReverseConnectionManager::connect(
    const QnRoute& route, std::chrono::milliseconds timeout)
{
    if (route.reverseConnect)
        return getProxySocket(route.id, timeout);

    auto socket = nx::network::SocketFactory::createStreamSocket(false);
    if (socket->connect(route.addr, nx::network::deprecated::kDefaultConnectTimeout))
        return socket;
    return std::unique_ptr<nx::network::AbstractStreamSocket>();
}

} // namespace network
} // namespace vms
} // namespace nx
