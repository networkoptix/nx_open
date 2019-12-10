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
#include <nx/network/url/url_builder.h>

static const size_t kProxyConnectionsToRequest = 3;
static const std::chrono::seconds kReverseConnectionTimeout(10);
static const std::chrono::minutes kPreparedSocketTimeout(1);
static const QByteArray kReverseConnectionListenerPath("/proxy-reverse");

namespace nx::vms::network {

ReverseConnectionManager::ReverseConnectionManager(QnHttpConnectionListener* tcpListener):
    AbstractServerConnector(tcpListener->commonModule()),
    m_tcpListener(tcpListener)
{
    tcpListener->addHandler<ReverseConnectionListener>(
        "HTTP", kReverseConnectionListenerPath.mid(1), this);
}

void ReverseConnectionManager::startReceivingNotifications(ec2::AbstractECConnection* connection)
{
    QObject::connect(
        connection, &ec2::AbstractECConnection::reverseConnectionRequested,
        this, &ReverseConnectionManager::onReverseConnectionRequest);
}

ReverseConnectionManager::~ReverseConnectionManager()
{
    this->disconnect();

    decltype (m_outgoingClients) outgoingClients;
    decltype (m_incomingConnections) incomingConnections;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(outgoingClients, m_outgoingClients);
        std::swap(incomingConnections, m_incomingConnections);
    }

    for (const auto& client: outgoingClients)
        client->pleaseStopSync();

    for (auto& [id, peer]: incomingConnections)
    {
        peer.promiseTimer.pleaseStopSync();

        for (auto& connection: peer.connections)
            connection->pleaseStopSync();

        for (auto& promise: peer.promises)
            promise.second.set_value(nullptr);
    }
}

void ReverseConnectionManager::onReverseConnectionRequest(
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
        // If remote server is not known yet try to use current server for authentication.
        // It work in case of remote server already know about current server.
        server = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    }

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
        const auto url = nx::network::url::Builder()
            .setScheme(server->getApiUrl().scheme())
            .setHost(route.addr.address.toString())
            .setPort(route.addr.port)
            .setPath(kReverseConnectionListenerPath).toUrl();
        httpClient->addAdditionalHeader(
            Qn::PROXY_SENDER_HEADER_NAME, commonModule()->moduleGUID().toSimpleByteArray());
        httpClient->doConnect(
            url,
            kReverseConnectionListenerPath,
            std::bind(&ReverseConnectionManager::onOutgoingConnectDone, this, httpClient.get()));

        m_outgoingClients.insert(std::move(httpClient));
    }
}

void ReverseConnectionManager::onOutgoingConnectDone(nx::network::http::AsyncClient* httpClient)
{
    QnMutexLocker lock(&m_mutex);
    for (auto itr = m_outgoingClients.begin(); itr != m_outgoingClients.end(); ++itr)
    {
        if (httpClient == itr->get())
        {
            nx::network::http::AsyncClient::State state = httpClient->state();
            if (state == nx::network::http::AsyncClient::State::sFailed || !httpClient->response())
            {
                    NX_WARNING(this, "Outgoing system error from %1: %2",
                        httpClient->url(), httpClient->lastSysErrorCode());
            }
            else if (httpClient->response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
            {
                NX_WARNING(this, "Outgoing HTTP error from %1: %2",
                    httpClient->url(), httpClient->response()->statusLine.statusCode);
            }
            else
            {
                NX_VERBOSE(this, "Save successful outgoing connection to %1 into listener",
                    httpClient->url());

                auto socket = httpClient->takeSocket();
                socket->setRecvTimeout(kReverseConnectionTimeout);
                socket->setNonBlockingMode(false);
                m_tcpListener->processNewConnection(std::move(socket));
            }
            m_outgoingClients.erase(itr);
            break;
        }
    }
}

bool ReverseConnectionManager::saveIncomingConnection(const QnUuid& peerId, Connection connection)
{
    QnMutexLocker lock(&m_mutex);
    const auto peer = m_incomingConnections.find(peerId);
    if (peer == m_incomingConnections.end() || peer->second.requested == 0)
    {
        NX_WARNING(this, "Incoming connection was not requested from %1", peerId);
        return false;
    }

    peer->second.requested -= 1;
    connection->setNonBlockingMode(true);
    if (!peer->second.promises.empty())
    {
        // TODO: configure timeout?
        peer->second.promises.begin()->second.set_value(std::move(connection));
        peer->second.promises.erase(peer->second.promises.begin());
        NX_VERBOSE(this, "Incoming connection from %1 is taken by promise (%2 still wating)",
            peerId, peer->second.promises.size());
        return true;
    }

    const auto connectionPtr = connection.get();
    peer->second.connections.push_back(std::move(connection));
    auto buffer = std::make_unique<QByteArray>();
    buffer->reserve(1);
    const auto bufferRawPtr = buffer.get();
    connectionPtr->setRecvTimeout(kPreparedSocketTimeout);
    connectionPtr->readSomeAsync(
        bufferRawPtr,
        [this, peerId, peer = &peer->second, connectionPtr, buffer = std::move(buffer)](
            SystemError::ErrorCode code, size_t /*size*/)
        {
            QnMutexLocker lock(&m_mutex);
            const auto it = std::find_if(peer->connections.begin(), peer->connections.end(),
                [&connectionPtr](const auto& c) { return c.get() == connectionPtr; });
            if (it == peer->connections.end())
                return; // Currently connection is being taken by client request.

            peer->connections.erase(it);
            NX_VERBOSE(this, "Incoming connection from %1 is closed: %2",
                peerId, SystemError::toString(code));
        });

    NX_VERBOSE(this, "Incoming connection from %1 is saved (%2 total)",
        peerId, peer->second.connections.size());
    return true;
}

cf::future<ReverseConnectionManager::Connection> ReverseConnectionManager::connect(
    const QnRoute& route,
    std::chrono::milliseconds timeout,
    bool sslRequired)
{
    NX_VERBOSE(this, "Connecting to %1...", route);
    if (route.reverseConnect)
        return reverseConnectTo(route.gatewayId.isNull() ? route.id : route.gatewayId, timeout);

    auto socket = nx::network::SocketFactory::createStreamSocket(sslRequired);
    socket->setSendTimeout(timeout);
    socket->setRecvTimeout(timeout);
    socket->setNonBlockingMode(true);

    cf::promise<std::unique_ptr<nx::network::AbstractStreamSocket>> promise;
    auto future = promise.get_future();
    socket.get()->connectAsync(
        route.addr,
        [this, route, socket = std::move(socket), promise = std::move(promise)](
            SystemError::ErrorCode code) mutable
        {
            NX_VERBOSE(this, "Connected directly to %1", route);
            if (code == SystemError::noError)
            {
                socket->cancelIOSync(nx::network::aio::etNone);
                return promise.set_value(std::move(socket));
            }

            NX_VERBOSE(this, "Unable to connect to %1: %2", route, SystemError::toString(code));
            socket.reset();
            promise.set_value(nullptr);
        });

    return future;
}

cf::future<ReverseConnectionManager::Connection> ReverseConnectionManager::reverseConnectTo(
    const QnUuid& peerId, std::chrono::milliseconds timeout)
{
    QnMutexLocker lock(&m_mutex);
    auto& peer = m_incomingConnections[peerId];
    if (!peer.connections.empty())
    {
        auto connection = std::move(peer.connections.front());
        peer.connections.pop_front();
        NX_VERBOSE(this, "Saved connection from %1 is taken (%2 left)",
            peerId, peer.connections.size());

        lock.unlock();
        connection->cancelIOSync(nx::network::aio::EventType::etNone);
        return cf::make_ready_future(std::move(connection));
    }

    cf::promise<std::unique_ptr<nx::network::AbstractStreamSocket>> promise;
    auto future = promise.get_future();
    const auto promiseIterator = peer.promises.emplace(
        std::chrono::steady_clock::now() + timeout, std::move(promise));

    if (promiseIterator == peer.promises.begin())
        restartPromiseTimer(peerId, &peer, timeout);

    requestReverseConnections(peerId, &peer);
    return future;
}

void ReverseConnectionManager::restartPromiseTimer(
    const QnUuid& peerId, IncomingConnections* peer, std::chrono::milliseconds timeout)
{
    peer->promiseTimer.dispatch(
        [this, peer, peerId, timeout]()
        {
            peer->promiseTimer.start(
                timeout,
                [this, peer, peerId]()
                {
                    const auto now = std::chrono::steady_clock::now();
                    QnMutexLocker lock(&m_mutex);
                    while (!peer->promises.empty() && peer->promises.begin()->first <= now)
                    {
                        NX_VERBOSE(this, "Notify expired promise for %1", peerId);
                        peer->promises.begin()->second.set_value(nullptr);
                        peer->promises.erase(peer->promises.begin());
                    }

                    if (!peer->promises.empty())
                    {
                        const auto tileLeft = peer->promises.begin()->first - now;
                        restartPromiseTimer(
                            peerId, peer, std::chrono::ceil<std::chrono::milliseconds>(tileLeft));
                    }
                });
        });
}

void ReverseConnectionManager::requestReverseConnections(
    const QnUuid& peerId, IncomingConnections* peer)
{
    if (peer->requested && peer->requestTimer.hasExpired(kReverseConnectionTimeout))
    {
        NX_WARNING(this, "%1 requested connection from %2 were never satisfied",
            peer->requested, peerId);
        peer->requested = 0;
    }

    if (peer->requested > peer->promises.size())
    {
        NX_VERBOSE(this, "Still waiting for %1 connections from %2", peer->requested, peerId);
        return;
    }

    api::ReverseConnectionData request;
    request.targetServer = commonModule()->moduleGUID();
    request.socketCount = (int) std::max(
        peer->promises.size() - peer->requested, kProxyConnectionsToRequest);

    ec2::QnTransaction<nx::vms::api::ReverseConnectionData> transaction(
        ec2::ApiCommand::openReverseConnection, request.targetServer, request);

    NX_DEBUG(this, "Requesting %1 connections from %2", request.socketCount, peerId);
    commonModule()->ec2Connection()->messageBus()->sendTransaction(transaction, peerId);

    peer->requested += (size_t) request.socketCount;
    peer->requestTimer.restart();
}

} // namespace nx::vms::network
