#include "peer_registrator.h"

#include <functional>
#include <iostream>

#include <common/common_globals.h>
#include <nx/utils/log/log.h>
#include <nx/network/stun/cc/custom_stun.h>

#include "listening_peer_pool.h"
#include "data/listening_peer.h"

namespace nx {
namespace hpm {

PeerRegistrator::PeerRegistrator(
    const conf::Settings& settings,
    AbstractCloudDataProvider* cloudData,
    nx::stun::MessageDispatcher* dispatcher,
    ListeningPeerPool* const listeningPeerPool)
:
    RequestProcessor(cloudData),
    m_settings(settings),
    m_listeningPeerPool(listeningPeerPool)
{
    using namespace std::placeholders;
    const auto result =
        dispatcher->registerRequestProcessor(
            stun::cc::methods::bind,
            [this](const ConnectionStrongRef& connection, stun::Message message)
                { bind( std::move(connection), std::move( message ) ); } ) &&

        dispatcher->registerRequestProcessor(
            stun::cc::methods::listen,
            [this](const ConnectionStrongRef& connection, stun::Message message)
            {
                processRequestWithNoOutput(
                    &PeerRegistrator::listen,
                    this,
                    std::move(connection),
                    std::move(message));
            } ) &&

        dispatcher->registerRequestProcessor(
            stun::cc::methods::resolveDomain,
            [this](const ConnectionStrongRef& connection, stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::resolveDomain,
                    this,
                    std::move(connection),
                    std::move(message));
            }) &&

        dispatcher->registerRequestProcessor(
            stun::cc::methods::resolvePeer,
            [this](const ConnectionStrongRef& connection, stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::resolvePeer,
                    this,
                    std::move(connection),
                    std::move(message));
            }) &&

        dispatcher->registerRequestProcessor(
            stun::cc::methods::clientBind,
            [this](const ConnectionStrongRef& connection, stun::Message message)
            {
                processRequestWithNoOutput(
                    &PeerRegistrator::clientBind,
                    this,
                    std::move(connection),
                    std::move(message));
            }) ;

    // TODO: NX_LOG
    NX_ASSERT(result, Q_FUNC_INFO, "Could not register one of processors");
}

data::ListeningPeers PeerRegistrator::getListeningPeers() const
{
    data::ListeningPeers result;
    result.systems = m_listeningPeerPool->getListeningPeers();

    QnMutexLocker lk(&m_mutex);
    for (const auto& client: m_boundClients)
    {
        data::BoundClientInfo info;
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
    stun::Message requestMessage)
{
    if (connection->transportProtocol() != nx::network::TransportProtocol::tcp)
        return sendErrorResponse(
            connection,
            requestMessage.header,
            api::ResultCode::badTransport,
            stun::error::badRequest,
            "Only tcp is allowed for bind request");

    MediaserverData mediaserverData;
    nx::String errorMessage;
    const api::ResultCode resultCode = 
        getMediaserverData(connection, requestMessage, &mediaserverData, &errorMessage);
    if (resultCode != api::ResultCode::ok)
    {
        sendErrorResponse(
            connection,
            requestMessage.header,
            resultCode,
            api::resultCodeToStunErrorCode(resultCode),
            errorMessage);
        return;
    }

    auto peerDataLocker = m_listeningPeerPool->insertAndLockPeerData(
        connection,
        mediaserverData);
    //TODO #ak if peer has already been bound with another connection, overwriting it...
    //peerDataLocker.value().peerConnection = connection;
    if (const auto attr = requestMessage.getAttribute< stun::cc::attrs::PublicEndpointList >())
        peerDataLocker.value().endpoints = attr->get();
    else
        peerDataLocker.value().endpoints.clear();

    NX_LOGX(lit("Peer %2.%3 succesfully bound, endpoints=%4")
        .arg(QString::fromUtf8(mediaserverData.systemId))
        .arg(QString::fromUtf8(mediaserverData.serverId))
        .arg(containerString(peerDataLocker.value().endpoints)), cl_logDEBUG1);

    sendSuccessResponse(connection, requestMessage.header);
}

void PeerRegistrator::listen(
    const ConnectionStrongRef& connection,
    api::ListenRequest requestData,
    stun::Message requestMessage,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (connection->transportProtocol() != nx::network::TransportProtocol::tcp)
        return completionHandler(api::ResultCode::badTransport);    //Only tcp is allowed for listen request

    MediaserverData mediaserverData;
    nx::String errorMessage;
    const api::ResultCode resultCode =
        getMediaserverData(connection, requestMessage, &mediaserverData, &errorMessage);
    if (resultCode != api::ResultCode::ok)
    {
        sendErrorResponse(
            connection,
            requestMessage.header,
            resultCode,
            api::resultCodeToStunErrorCode(resultCode),
            errorMessage);
        return;
    }

    std::vector<nx::stun::Message> clientBindIndications;
    {
        QnMutexLocker lk(&m_mutex);
        for (const auto& client: m_boundClients)
            clientBindIndications.push_back(makeIndication(client.first, client.second));

        auto peerDataLocker = m_listeningPeerPool->insertAndLockPeerData(
            connection,
            MediaserverData(requestData.systemId, requestData.serverId));

        peerDataLocker.value().isListening = true;
        peerDataLocker.value().connectionMethods |= api::ConnectionMethod::udpHolePunching;

        NX_LOGX(lit("Peer %1.%2 started to listen")
            .arg(QString::fromUtf8(requestData.serverId))
            .arg(QString::fromUtf8(requestData.systemId)),
            cl_logDEBUG1);
    }

    completionHandler(api::ResultCode::ok);
    for (auto& indication: clientBindIndications)
        connection->sendMessage(std::move(indication));
}

void PeerRegistrator::resolveDomain(
    const ConnectionStrongRef& connection,
    api::ResolveDomainRequest requestData,
    stun::Message /*requestMessage*/,
    std::function<void(
        api::ResultCode, api::ResolveDomainResponse)> completionHandler)
{
    const auto peers = m_listeningPeerPool->findPeersBySystemId(
                requestData.domainName);
    if (peers.empty())
    {
        NX_LOGX(lm("Could not resolve domain %1. client address: %2")
            .arg(requestData.domainName)
            .arg(connection->getSourceAddress().toString()), cl_logDEBUG2);

        return completionHandler(
            api::ResultCode::notFound,
            api::ResolveDomainResponse());
    }

    api::ResolveDomainResponse responseData;
    for (const auto& peer : peers)
        responseData.hostNames.push_back(peer.hostName());

    NX_LOGX(lm("Successfully resolved domain %1 (requested from %2), hostNames=%3")
        .arg(requestData.domainName).arg(connection->getSourceAddress().toString())
        .arg(containerString(peers)), cl_logDEBUG2);

    completionHandler(api::ResultCode::ok, std::move(responseData));
}

void PeerRegistrator::resolvePeer(
    const ConnectionStrongRef& connection,
    api::ResolvePeerRequest requestData,
    stun::Message /*requestMessage*/,
    std::function<void(
        api::ResultCode, api::ResolvePeerResponse)> completionHandler)
{
    auto peerDataLocker = m_listeningPeerPool->findAndLockPeerDataByHostName(
        requestData.hostName);
    if (!static_cast<bool>(peerDataLocker))
    {
        NX_LOGX(lm("Could not resolve host %1. client address: %2")
            .arg(requestData.hostName)
            .arg(connection->getSourceAddress().toString()), cl_logDEBUG2);

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

    NX_LOGX(lm("Successfully resolved host %1 (requested from %2)")
        .str(peerDataLocker->value())
        .arg(connection->getSourceAddress().toString()), cl_logDEBUG2);

    completionHandler(api::ResultCode::ok, std::move(responseData));
}

void PeerRegistrator::clientBind(
    const ConnectionStrongRef& connection,
    api::ClientBindRequest requestData,
    stun::Message /*requestMessage*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    const auto reject = [&](api::ResultCode code)
    {
        NX_LOGX(lm("Reject client bind (requested from %1): %2")
            .strs(connection->getSourceAddress(), code), cl_logDEBUG2);

        completionHandler(code);
    };

    // Only local peers are alowed while auth is not avaliable for clients:
    if (!connection->getSourceAddress().address.isLocal())
        return reject(api::ResultCode::notAuthorized);

    if (requestData.tcpReverseEndpoints.empty())
        return reject(api::ResultCode::badRequest);

    auto peerId = std::move(requestData.originatingPeerID);
    nx::stun::Message indication;
    std::vector<ConnectionWeakRef> listeningPeerConnections;
    {
        QnMutexLocker lk(&m_mutex);
        ClientBindInfo& info = m_boundClients[peerId];
        info.connection = connection;
        info.tcpReverseEndpoints = std::move(requestData.tcpReverseEndpoints);

        NX_LOGX(lm("Successfully bound client %1 with tcpReverseEndpoints=%2 (requested from %3)")
            .str(peerId).container(info.tcpReverseEndpoints)
            .str(connection->getSourceAddress()), cl_logDEBUG1);

        indication = makeIndication(peerId, info);
        listeningPeerConnections = m_listeningPeerPool->getAllConnections();
    }

    connection->addOnConnectionCloseHandler(
        [this, peerId, connection]()
        {
            QnMutexLocker lk(&m_mutex);
            const auto it = m_boundClients.find(peerId);
            if (it == m_boundClients.end() || it->second.connection.lock() != connection)
                return;

            NX_LOGX(lm("Client %1 has disconnected").str(peerId), cl_logDEBUG1);
            m_boundClients.erase(it);
        });

    completionHandler(api::ResultCode::ok);
    for (const auto& connectionRef: listeningPeerConnections)
        if (const auto connection = connectionRef.lock())
            connection->sendMessage(indication);
}

nx::stun::Message PeerRegistrator::makeIndication(const String& id, const ClientBindInfo& info) const
{
    api::ConnectionRequestedEvent event;
    event.originatingPeerID = id;
    event.tcpReverseEndpointList = info.tcpReverseEndpoints;
    event.connectionMethods = api::ConnectionMethod::reverseConnect; // TODO: Support others?
    event.params = m_settings.connectionParameters();
    event.isPersistent = true;

    nx::stun::Message indication(stun::Header(stun::MessageClass::indication, event.kMethod));
    event.serialize(&indication);
    return indication;
}

} // namespace hpm
} // namespace nx
