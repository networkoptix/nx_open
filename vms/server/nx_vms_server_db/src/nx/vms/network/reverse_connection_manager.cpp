#include "reverse_connection_manager.h"

#include <algorithm>

#include <nx/utils/log/log.h>
#include <transaction/transaction.h>
#include <common/common_module.h>
#include <transaction/message_bus_adapter.h>
#include "reverse_connection_listener.h"
#include <nx/utils/scope_guard.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/log/log_main.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/custom_headers.h>

namespace {

static const int kProxyKeepAliveIntervalMs = 40 * 1000;
static const int kProxyConnectionsToRequest = 3;
static const std::chrono::seconds kReverseConnectionTimeout(10);
static const std::chrono::minutes kPreperedSocketTimeout(1);
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
}

void ReverseConnectionManager::startReceivingNotifications(ec2::AbstractECConnection* connection)
{
    connect(
        connection, &ec2::AbstractECConnection::reverseConnectionRequested,
        this, &ReverseConnectionManager::at_reverseConnectionRequested);
}

ReverseConnectionManager::~ReverseConnectionManager()
{
    decltype (m_runningHttpClients) httpClients;
    decltype (m_preparedSockets) preparedSockets;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(httpClients, m_runningHttpClients);
        std::swap(preparedSockets, m_preparedSockets);
    }
    for (const auto& client: httpClients)
        client->pleaseStopSync();
    preparedSockets.clear();
}

void ReverseConnectionManager::at_reverseConnectionRequested(
    const nx::vms::api::ReverseConnectionData& data)
{
    QnMutexLocker lock(&m_mutex);

    NX_DEBUG(this, lm("Got incoming request. %1 connection(s) to the target server %2 is(are) requested.")
        .arg(data.socketCount).arg(data.targetServer.toString()));

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
        NX_WARNING(this, lm("Target server %1 is not known yet")
            .arg(qnStaticCommon->moduleDisplayName(data.targetServer)));
        return;
    }

    for (int i = 0; i < data.socketCount; ++i)
    {
        auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
        httpClient->setUserName(server->getId().toByteArray());
        httpClient->setUserPassword(server->getAuthKey().toUtf8());

        httpClient->setSendTimeout(kReverseConnectionTimeout);
        httpClient->setResponseReadTimeout(kReverseConnectionTimeout);
        nx::utils::Url url(server->getApiUrl());
        url.setHost(route.addr.address.toString());
        url.setPort(route.addr.port);
        url.setPath(kReverseConnectionListenerPath);
        httpClient->addAdditionalHeader(
            Qn::PROXY_SENDER_HEADER_NAME, commonModule()->moduleGUID().toSimpleByteArray());
        httpClient->doConnect(
            url,
            kReverseConnectionListenerPath,
            std::bind(&ReverseConnectionManager::onHttpClientDone, this, httpClient.get()));

        m_runningHttpClients.insert(std::move(httpClient));
    }
}

void ReverseConnectionManager::onHttpClientDone(nx::network::http::AsyncClient* httpClient)
{
    QnMutexLocker lock(&m_mutex);
    for (auto itr = m_runningHttpClients.begin(); itr != m_runningHttpClients.end(); ++itr)
    {
        if (httpClient == itr->get())
        {
            nx::network::http::AsyncClient::State state = httpClient->state();
            if (state == nx::network::http::AsyncClient::State::sFailed)
            {
                NX_WARNING(this,
                    lm("Failed to establish reverse connection to the target server=%1").arg(httpClient->url()));
            }
            else
            {
                auto socket = httpClient->takeSocket();
                socket->setRecvTimeout(kReverseConnectionTimeout);
                socket->setNonBlockingMode(false);
                m_tcpListener->processNewConnection(std::move(socket));
            }
            m_runningHttpClients.erase(itr);
            break;
        }
    }
}

ReverseConnectionManager::SocketData
    ReverseConnectionManager::getPreparedSocketUnsafe(const QnUuid& guid)
{
    auto& socketPool = m_preparedSockets[guid];
    if (!socketPool.sockets.empty())
    {
        auto result = std::move(socketPool.sockets.front());
        socketPool.sockets.pop_front();
        return result;
    }
    return SocketData();
}

std::unique_ptr<nx::network::AbstractStreamSocket> ReverseConnectionManager::getProxySocket(
    const QnUuid& guid, std::chrono::milliseconds timeout)
{
    NX_DEBUG(this, lit("Reverse connection to %1 is requested").arg(guid.toString()));

    auto doSocketRequest = [&](int socketCount)
    {
        ec2::QnTransaction<nx::vms::api::ReverseConnectionData> tran(
            ec2::ApiCommand::openReverseConnection,
            commonModule()->moduleGUID());
        tran.params.targetServer = commonModule()->moduleGUID();
        tran.params.socketCount = socketCount;
        commonModule()->ec2Connection()->messageBus()->sendTransaction(tran, guid);
    };

    QnMutexLocker lock(&m_mutex);
    nx::utils::ElapsedTimer timer;
    timer.restart();
    while (!timer.hasExpired(kReverseConnectionTimeout))
    {
        auto socketData = getPreparedSocketUnsafe(guid);
        if (socketData.socket)
        {
            lock.unlock();
            socketData.socket->cancelIOSync(nx::network::aio::etRead);
            return std::move(socketData.socket);
        }
        auto& socketPool = m_preparedSockets[guid];
        if (socketPool.requested == 0 || socketPool.timer.hasExpired(kReverseConnectionTimeout))
        {
            doSocketRequest(kProxyConnectionsToRequest);
            socketPool.requested = kProxyConnectionsToRequest;
            socketPool.timer.restart();
        }
        m_proxyCondition.wait(&m_mutex, std::chrono::milliseconds(kReverseConnectionTimeout).count());
    }
    return std::unique_ptr<nx::network::AbstractStreamSocket>();
}

bool ReverseConnectionManager::addIncomingTcpConnection(
    const QString& guid, std::unique_ptr<nx::network::AbstractStreamSocket> socket)
{
    QnMutexLocker lock(&m_mutex);
    auto socketPool = m_preparedSockets.find(QnUuid(guid));
    if (socketPool == m_preparedSockets.end() || socketPool->second.requested == 0)
    {
        NX_WARNING(this, lit("Reverse connection was not requested from %1").arg(guid));
        return false;
    }

    --socketPool->second.requested;

    using namespace std::placeholders;
    socket->setNonBlockingMode(true);
    socket->setRecvTimeout(kPreperedSocketTimeout);

    SocketData data;
    data.socket = std::move(socket);
    data.socket->readSomeAsync(data.tmpReadBuffer.get(),
        std::bind(&ReverseConnectionManager::at_socketReadTimeout, this, QnUuid(guid), data.socket.get(), _1, _2));

    socketPool->second.sockets.push_back(std::move(data));

    NX_DEBUG(this, lit(
        "Got new reverse connection from %1, there is (are) %2 avaliable and %3 requested")
        .arg(guid).arg(socketPool->second.sockets.size()));

    m_proxyCondition.wakeAll();
    return true;
}

void ReverseConnectionManager::at_socketReadTimeout(
    const QnUuid& serverId,
    nx::network::AbstractStreamSocket* socket,
    SystemError::ErrorCode /*errorCode*/,
    size_t /*bytesRead*/)
{
    QnMutexLocker lock(&m_mutex);
    auto& preparedData = m_preparedSockets[serverId];
    preparedData.sockets.erase(std::remove_if(
        preparedData.sockets.begin(), preparedData.sockets.end(),
        [socket](const SocketData& data)
        {
            return data.socket.get() == socket;
        }), preparedData.sockets.end());

    m_proxyCondition.wakeAll();
}

std::unique_ptr<nx::network::AbstractStreamSocket> ReverseConnectionManager::connectTo(
    const QnRoute& route,
    bool sslRequired,
    std::chrono::milliseconds timeout)
{
    if (route.reverseConnect)
    {
        const auto serverId = route.gatewayId.isNull() ? route.id : route.gatewayId;
        return getProxySocket(serverId, timeout);
    }

    auto socket = nx::network::SocketFactory::createStreamSocket(sslRequired);
    if (socket->connect(route.addr, nx::network::deprecated::kDefaultConnectTimeout))
        return socket;
    return std::unique_ptr<nx::network::AbstractStreamSocket>();
}

} // namespace network
} // namespace vms
} // namespace nx
