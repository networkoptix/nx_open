#include "peer_registrator.h"

#include <functional>
#include <iostream>

#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

#include "listening_peer_pool.h"
#include "relay/abstract_relay_cluster_client.h"

namespace nx {
namespace hpm {

PeerRegistrator::PeerRegistrator(
    const conf::Settings& settings,
    AbstractCloudDataProvider* cloudData,
    ListeningPeerPool* const listeningPeerPool,
    AbstractRelayClusterClient* const relayClusterClient)
:
    RequestProcessor(cloudData),
    m_settings(settings),
    m_listeningPeerPool(listeningPeerPool),
    m_relayClusterClient(relayClusterClient)
{
}

PeerRegistrator::~PeerRegistrator()
{
    m_counter.wait();
    m_asyncOperationGuard->terminate();
}

api::ListeningPeers PeerRegistrator::getListeningPeers() const
{
    api::ListeningPeers result;
    result.systems = m_listeningPeerPool->getListeningPeers();

    QnMutexLocker lk(&m_mutex);
    for (const auto& client: m_boundClients)
    {
        api::BoundClient info;
        if (const auto connetion = client.second.connection.lock())
            info.connectionEndpoint = connetion->getSourceAddress().toString();

        for (const auto& endpoint: client.second.tcpReverseEndpoints)
            info.tcpReverseEndpoints.push_back(endpoint.toString());

        result.clients.emplace(QString::fromUtf8(client.first), std::move(info));
    }

    return result;
}

void PeerRegistrator::bind(
    const ConnectionStrongRef& connection,
    network::stun::Message requestMessage)
{
    auto serverConnection =
        std::dynamic_pointer_cast<network::stun::ServerConnection>(connection);

    if (connection->transportProtocol() != nx::network::TransportProtocol::tcp ||
        !serverConnection)
    {
        return sendErrorResponse(
            connection,
            requestMessage.header,
            api::ResultCode::badTransport,
            network::stun::error::badRequest,
            "Only tcp is allowed for bind request");
    }

    MediaserverData mediaserverData;
    nx::String errorMessage;
    const api::ResultCode resultCode =
        getMediaserverData(*serverConnection, requestMessage, &mediaserverData, &errorMessage);
    if (resultCode != api::ResultCode::ok)
    {
        sendErrorResponse(
            serverConnection,
            requestMessage.header,
            resultCode,
            api::resultCodeToStunErrorCode(resultCode),
            errorMessage);
        return;
    }

    auto peerDataLocker = m_listeningPeerPool->insertAndLockPeerData(
        serverConnection,
        mediaserverData);
    //TODO #ak if peer has already been bound with another connection, overwriting it...
    //peerDataLocker.value().peerConnection = connection;
    if (const auto attr = requestMessage.getAttribute< network::stun::extension::attrs::PublicEndpointList >())
        peerDataLocker.value().endpoints = attr->get();
    else
        peerDataLocker.value().endpoints.clear();

    NX_DEBUG(this, lit("Peer %2.%3 succesfully bound, endpoints=%4")
        .arg(QString::fromUtf8(mediaserverData.systemId))
        .arg(QString::fromUtf8(mediaserverData.serverId))
        .arg(containerString(peerDataLocker.value().endpoints)));

    sendSuccessResponse(serverConnection, requestMessage.header);
}

void PeerRegistrator::listen(
    const ConnectionStrongRef& connection,
    network::stun::Message requestMessage,
    api::ListenRequest requestData,
    std::function<void(api::ResultCode, api::ListenResponse)> completionHandler)
{
    if (connection->transportProtocol() != nx::network::TransportProtocol::tcp)
        return completionHandler(api::ResultCode::badTransport, {});    //Only tcp is allowed for listen request

    auto serverConnection =
        std::dynamic_pointer_cast<network::stun::ServerConnection>(connection);

    MediaserverData mediaserverData;
    nx::String errorMessage;
    const api::ResultCode resultCode =
        getMediaserverData(*serverConnection, requestMessage, &mediaserverData, &errorMessage);
    if (resultCode != api::ResultCode::ok)
        return completionHandler(resultCode, {});

    const MediaserverData mediaserverConnectionKey(
        requestData.systemId, requestData.serverId);
    auto peerDataLocker = m_listeningPeerPool->insertAndLockPeerData(
        serverConnection,
        mediaserverConnectionKey);

    peerDataLocker.value().isListening = true;
    peerDataLocker.value().cloudConnectVersion = requestData.cloudConnectVersion;

    // TODO: #ak Should not mark as listening before sending LISTEN response.

    NX_DEBUG(this, lm("Peer %1.%2 started to listen")
        .args(requestData.serverId, requestData.systemId));

    m_relayClusterClient->selectRelayInstanceForListeningPeer(
        lm("%1.%2").arg(requestData.serverId).arg(requestData.systemId).toStdString(),
        [this, serverConnectionAioThread = serverConnection->getAioThread(),
            completionHandler = std::move(completionHandler), mediaserverConnectionKey,
            asyncCallLocker = m_counter.getScopedIncrement()](
                nx::cloud::relay::api::ResultCode resultCode,
                QUrl relayInstanceUrl) mutable
        {
            // Sending response from connection's aio thread.
            auto aioCaller = std::make_unique<nx::network::aio::BasicPollable>();
            aioCaller->bindToAioThread(serverConnectionAioThread);
            auto aioCallerPtr = aioCaller.get();

            // TODO: #ak Code here is a mess. Major refactoring is needed.
            // Likely, it is needed to get rid of shared connections to simplify it.

            aioCallerPtr->post(
                [this, aioCaller = std::move(aioCaller), mediaserverConnectionKey,
                    asyncCallLocker = std::move(asyncCallLocker), resultCode,
                    relayInstanceUrl, completionHandler = std::move(completionHandler)]() mutable
                {
                    sendListenResponse(
                        resultCode == nx::cloud::relay::api::ResultCode::ok
                            ? boost::make_optional(relayInstanceUrl)
                            : boost::none,
                        std::move(completionHandler));

                    reportClientBind(mediaserverConnectionKey);
                });
        });
}

void PeerRegistrator::checkOwnState(
    const ConnectionStrongRef& connection,
    network::stun::Message requestMessage,
    api::GetConnectionStateRequest /*requestData*/,
    std::function<void(api::ResultCode, api::GetConnectionStateResponse)> completionHandler)
{
    MediaserverData mediaserverData;
    nx::String errorMessage;
    const api::ResultCode resultCode =
        getMediaserverData(*connection, requestMessage, &mediaserverData, &errorMessage);
    if (resultCode != api::ResultCode::ok)
        return completionHandler(resultCode, {});

    api::GetConnectionStateResponse response;
    auto peer = m_listeningPeerPool->findAndLockPeerDataByHostName(mediaserverData.hostName());
    if (peer && peer->value().isListening)
        response.state = api::GetConnectionStateResponse::State::listening;

    completionHandler(api::ResultCode::ok, std::move(response));
}

void PeerRegistrator::resolveDomain(
    const ConnectionStrongRef& connection,
    api::ResolveDomainRequest requestData,
    std::function<void(
        api::ResultCode, api::ResolveDomainResponse)> completionHandler)
{
    const auto peers = m_listeningPeerPool->findPeersBySystemId(
                requestData.domainName);
    if (peers.empty())
    {
        NX_VERBOSE(this, lm("Could not resolve domain %1. client address: %2")
            .arg(requestData.domainName)
            .arg(connection->getSourceAddress().toString()));

        return completionHandler(
            api::ResultCode::notFound,
            api::ResolveDomainResponse());
    }

    api::ResolveDomainResponse responseData;
    for (const auto& peer : peers)
        responseData.hostNames.push_back(peer.hostName());

    NX_VERBOSE(this, lm("Successfully resolved domain %1 (requested from %2), hostNames=%3")
        .arg(requestData.domainName).arg(connection->getSourceAddress().toString())
        .arg(containerString(peers)));

    completionHandler(api::ResultCode::ok, std::move(responseData));
}

void PeerRegistrator::resolvePeer(
    const ConnectionStrongRef& connection,
    api::ResolvePeerRequest requestData,
    std::function<void(
        api::ResultCode, api::ResolvePeerResponse)> completionHandler)
{
    auto peerDataLocker = m_listeningPeerPool->findAndLockPeerDataByHostName(
        requestData.hostName);
    if (!static_cast<bool>(peerDataLocker))
    {
        NX_VERBOSE(this, lm("Could not resolve host %1. client address: %2")
            .arg(requestData.hostName)
            .arg(connection->getSourceAddress().toString()));

        return completionHandler(
            api::ResultCode::notFound,
            api::ResolvePeerResponse());
    }

    api::ResolvePeerResponse responseData;

    if (!peerDataLocker->value().endpoints.empty())
        responseData.endpoints = peerDataLocker->value().endpoints;
    if (peerDataLocker->value().isListening)
        responseData.connectionMethods =
            api::ConnectionMethod::udpHolePunching |
            api::ConnectionMethod::proxy;

    NX_VERBOSE(this, lm("Successfully resolved host %1 (requested from %2)")
        .arg(peerDataLocker->value())
        .arg(connection->getSourceAddress().toString()));

    completionHandler(api::ResultCode::ok, std::move(responseData));
}

void PeerRegistrator::clientBind(
    const ConnectionStrongRef& connection,
    api::ClientBindRequest requestData,
    std::function<void(api::ResultCode, api::ClientBindResponse)> completionHandler)
{
    const auto reject = [&](api::ResultCode code)
    {
        NX_VERBOSE(this, lm("Reject client bind (requested from %1): %2")
            .args(connection->getSourceAddress(), code));

        completionHandler(code, {});
    };

    // Only local peers are alowed while auth is not avaliable for clients:
    if (!connection->getSourceAddress().address.isLocalNetwork())
        return reject(api::ResultCode::notAuthorized);

    if (requestData.tcpReverseEndpoints.empty())
        return reject(api::ResultCode::badRequest);

    auto peerId = std::move(requestData.originatingPeerID);
    network::stun::Message indication;
    std::vector<ConnectionWeakRef> listeningPeerConnections;
    {
        QnMutexLocker lk(&m_mutex);
        ClientBindInfo& info = m_boundClients[peerId];
        connection->setInactivityTimeout(std::nullopt);
        info.connection = connection;
        info.tcpReverseEndpoints = std::move(requestData.tcpReverseEndpoints);

        NX_DEBUG(this, lm("Successfully bound client %1 with tcpReverseEndpoints=%2 (requested from %3)")
            .arg(peerId).container(info.tcpReverseEndpoints)
            .arg(connection->getSourceAddress()));

        indication = makeIndication(peerId, info);
        listeningPeerConnections = m_listeningPeerPool->getAllConnections();
    }

    connection->addOnConnectionCloseHandler(
        [this, peerId, connection, guard = m_asyncOperationGuard.sharedGuard()](
            SystemError::ErrorCode /*closeReason*/)
        {
            // TODO: #ak Logic here seems to duplicate ListeningPeerPool.
            // The only difference is here we have client connection, there - server connection.

            auto lock = guard->lock();
            if (!lock)
                return; //< ListeningPeerPool has been destroyed.

            QnMutexLocker lk(&m_mutex);
            const auto it = m_boundClients.find(peerId);
            if (it == m_boundClients.end() || it->second.connection.lock() != connection)
                return;

            NX_DEBUG(this, lm("Client %1 has disconnected").arg(peerId));
            m_boundClients.erase(it);
        });

    api::ClientBindResponse response;
    response.tcpConnectionKeepAlive = m_settings.stun().keepAliveOptions;
    completionHandler(api::ResultCode::ok, std::move(response));

    for (const auto& connectionRef: listeningPeerConnections)
        if (const auto connection = connectionRef.lock())
            connection->sendMessage(indication);
}

int PeerRegistrator::boundClientCount() const
{
    QnMutexLocker lk(&m_mutex);
    return (int) m_boundClients.size();
}

void PeerRegistrator::sendListenResponse(
    boost::optional<QUrl> trafficRelayInstanceUrl,
    std::function<void(api::ResultCode, api::ListenResponse)> responseSender)
{
    api::ListenResponse response;
    if (trafficRelayInstanceUrl)
        response.trafficRelayUrl = trafficRelayInstanceUrl->toString().toUtf8();
    response.tcpConnectionKeepAlive = m_settings.stun().keepAliveOptions;
    response.cloudConnectOptions = m_settings.general().cloudConnectOptions;
    responseSender(api::ResultCode::ok, std::move(response));
}

void PeerRegistrator::reportClientBind(
    const MediaserverData& mediaserverConnectionKey)
{
    std::shared_ptr<nx::network::stun::ServerConnection> peerConnection;

    {
        auto peerLocker = m_listeningPeerPool->findAndLockPeerDataByHostName(
            mediaserverConnectionKey.hostName());
        if (!peerLocker)
            return;
        peerConnection = peerLocker->value().peerConnection;
    }

    auto clientBindIndications = prepareClientBindIndications();
    peerConnection->dispatch(
        [this, clientBindIndications = std::move(clientBindIndications),
            peerConnectionPtr = peerConnection.get()]() mutable
        {
            for (auto& indication: clientBindIndications)
                peerConnectionPtr->sendMessage(std::move(indication), [](auto...) {});
        });
}

std::vector<network::stun::Message> PeerRegistrator::prepareClientBindIndications()
{
    QnMutexLocker lk(&m_mutex);

    std::vector<network::stun::Message> clientBindIndications;
    for (const auto& client: m_boundClients)
        clientBindIndications.push_back(makeIndication(client.first, client.second));

    return clientBindIndications;
}

network::stun::Message PeerRegistrator::makeIndication(
    const String& id,
    const ClientBindInfo& info) const
{
    api::ConnectionRequestedEvent event;
    event.originatingPeerID = id;
    event.tcpReverseEndpointList = info.tcpReverseEndpoints;
    event.connectionMethods = api::ConnectionMethod::reverseConnect; // TODO: Support others?
    event.params = m_settings.connectionParameters();
    event.isPersistent = true;

    network::stun::Message indication(
        network::stun::Header(
            network::stun::MessageClass::indication,
            event.kMethod));
    event.serialize(&indication);
    return indication;
}

} // namespace hpm
} // namespace nx
