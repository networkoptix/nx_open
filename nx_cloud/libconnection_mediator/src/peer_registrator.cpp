
#include "peer_registrator.h"

#include <functional>
#include <iostream>

#include <common/common_globals.h>
#include <nx/utils/log/log.h>
#include <nx/network/stun/cc/custom_stun.h>

#include "listening_peer_pool.h"


namespace nx {
namespace hpm {

PeerRegistrator::PeerRegistrator(
    AbstractCloudDataProvider* cloudData,
    nx::stun::MessageDispatcher* dispatcher,
    ListeningPeerPool* const listeningPeerPool)
:
    RequestProcessor(cloudData),
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
            });

        dispatcher->registerRequestProcessor(
            stun::cc::methods::resolvePeer,
            [this](const ConnectionStrongRef& connection, stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::resolvePeer,
                    this,
                    std::move(connection),
                    std::move(message));
            });

    // TODO: NX_LOG
    NX_ASSERT(result, Q_FUNC_INFO, "Could not register one of processors");
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

    const auto mediaserverData = getMediaserverData(connection, requestMessage);
    if (!static_cast<bool>(mediaserverData))
    {
        sendErrorResponse(
            connection,
            requestMessage.header,
            api::ResultCode::notAuthorized,
            stun::error::badRequest,
            "No mediaserver data in request");
        return;
    }

    auto peerDataLocker = m_listeningPeerPool->insertAndLockPeerData(
        connection,
        *mediaserverData);
    //TODO #ak if peer has already been bound with another connection, overwriting it...
    //peerDataLocker.value().peerConnection = connection;
    if (const auto attr = requestMessage.getAttribute< stun::cc::attrs::PublicEndpointList >())
        peerDataLocker.value().endpoints = attr->get();
    else
        peerDataLocker.value().endpoints.clear();

    NX_LOGX(lit("Peer %2.%3 succesfully bound, endpoints=%4")
        .arg(QString::fromUtf8(mediaserverData->systemId))
        .arg(QString::fromUtf8(mediaserverData->serverId))
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

    //TODO #ak make authentication centralized
    const auto mediaserverData = getMediaserverData(connection, requestMessage);
    if (!static_cast<bool>(mediaserverData))
    {
        sendErrorResponse(
            connection,
            requestMessage.header,
            api::ResultCode::notAuthorized,
            stun::error::badRequest,
            "Bad system credentials supplied");
        return;
    }

    auto peerDataLocker = m_listeningPeerPool->insertAndLockPeerData(
        connection,
        MediaserverData(
            requestData.systemId,
            requestData.serverId));

    peerDataLocker.value().isListening = true;
    peerDataLocker.value().connectionMethods |= api::ConnectionMethod::udpHolePunching;

    NX_LOGX(lit("Peer %1.%2 started to listen")
        .arg(QString::fromUtf8(requestData.systemId))
        .arg(QString::fromUtf8(requestData.serverId)),
        cl_logDEBUG1);

    completionHandler(api::ResultCode::ok);
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

    NX_LOGX(lm("Successfully resolved host %1 (requested from %2), endpoints=%3")
        .arg(requestData.hostName).arg(connection->getSourceAddress().toString())
        .arg(containerString(peerDataLocker->value().endpoints)),
        cl_logDEBUG2);

    completionHandler(api::ResultCode::ok, std::move(responseData));
}

} // namespace hpm
} // namespace nx
