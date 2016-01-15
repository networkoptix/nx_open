
#include "listening_peer_registrator.h"

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
                { listen( std::move(connection), std::move( message ) ); } ) &&

        dispatcher->registerRequestProcessor(
            stun::cc::methods::resolve,
            [this](const ConnectionStrongRef& connection, stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::resolve,
                    this,
                    std::move(connection),
                    std::move(message));
            });

    // TODO: NX_LOG
    Q_ASSERT_X(result, Q_FUNC_INFO, "Could not register one of processors");
}

void PeerRegistrator::bind(const ConnectionStrongRef& connection,
                             stun::Message message)
{
    if (connection->transportProtocol() != nx::network::TransportProtocol::tcp)
        return sendErrorResponse(
            connection,
            message.header,
            stun::error::badRequest,
            "Only tcp is allowed for bind request");

    const auto mediaserverData = getMediaserverData(connection, message);
    if (!static_cast<bool>(mediaserverData))
    {
        sendErrorResponse(connection, message.header, stun::error::badRequest,
            "No mediaserver data in request");
        return;
    }

    auto peerDataLocker = m_listeningPeerPool->findAndLockPeerData(
        connection,
        *mediaserverData);
    //TODO #ak if peer has already been bound with another connection, overwriting it...
    //peerDataLocker.value().peerConnection = connection;
    if (const auto attr = message.getAttribute< stun::cc::attrs::PublicEndpointList >())
        peerDataLocker.value().endpoints = attr->get();
    else
        peerDataLocker.value().endpoints.clear();

    NX_LOGX(lit("Peer %2.%3 succesfully bound, endpoints=%4")
        .arg(QString::fromUtf8(mediaserverData->systemId))
        .arg(QString::fromUtf8(mediaserverData->serverId))
        .arg(containerString(peerDataLocker.value().endpoints)), cl_logDEBUG1);

    sendSuccessResponse(connection, message.header);

}

void PeerRegistrator::listen(const ConnectionStrongRef& connection,
                               stun::Message message)
{
    if (connection->transportProtocol() != nx::network::TransportProtocol::tcp)
        return sendErrorResponse(
            connection,
            message.header,
            stun::error::badRequest,
            "Only tcp is allowed for listen request");

    const auto mediaserverData = getMediaserverData(connection, message);
    if (!static_cast<bool>(mediaserverData))
    {
        sendErrorResponse(connection, message.header, stun::error::badRequest,
            "No mediaserver data in request");
        return;
    }

    auto peerDataLocker = m_listeningPeerPool->findAndLockPeerData(
        connection,
        *mediaserverData);
    peerDataLocker.value().isListening = true;

    NX_LOGX(lit("Peer %1.%2 started to listen")
        .arg(QString::fromUtf8(mediaserverData->systemId))
        .arg(QString::fromUtf8(mediaserverData->serverId)),
        cl_logDEBUG1);

    sendSuccessResponse(connection, message.header);

}

void PeerRegistrator::resolve(
    const ConnectionStrongRef& connection,
    api::ResolveRequest requestData,
    std::function<void(api::ResultCode, api::ResolveResponse)> completionHandler)
{
    auto peerDataLocker = m_listeningPeerPool->findAndLockPeerDataByHostName(
        requestData.hostName);
    if (!static_cast<bool>(peerDataLocker))
    {
        NX_LOGX(lm("Could not resolve host %1. client address: %2")
            .arg(requestData.hostName).arg(connection->getSourceAddress().toString()),
            cl_logDEBUG2);

        return completionHandler(
            api::ResultCode::notFound,
            api::ResolveResponse());
    }

    api::ResolveResponse responseData;

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

//void PeerRegistrator::connect(const ConnectionStrongRef& connection,
//                                stun::Message message )
//{
//    const auto userNameAttr = message.getAttribute< stun::cc::attrs::PeerId >();
//    if( !userNameAttr )
//        return sendErrorResponse( connection, message.header, stun::error::badRequest,
//            "Attribute ClientId is required" );
//
//    const auto hostNameAttr = message.getAttribute< stun::cc::attrs::HostName >();
//    if( !hostNameAttr )
//        return sendErrorResponse( connection, message.header, stun::error::badRequest,
//            "Attribute HostName is required" );
//
//    const auto userName = userNameAttr->getString();
//    const auto hostName = hostNameAttr->getString();
//
//    QnMutexLocker lk( &m_mutex );
//    if (const auto peer = m_peers.search(hostName))
//    {
//        stun::Message response( stun::Header(
//            stun::MessageClass::successResponse, message.header.method,
//            std::move( message.header.transactionId ) ) );
//
//        if( !peer->endpoints.empty() )
//            response.newAttribute< stun::cc::attrs::PublicEndpointList >( peer->endpoints );
//
//        if( !peer->isListening )
//            { /* TODO: StunEndpointList */ }
//
//        NX_LOGX( lit("Client %1 connects to %2, endpoints=%3")
//                .arg( QString::fromUtf8( userName ) )
//                .arg( QString::fromUtf8( hostName ) )
//                .arg( containerString( peer->endpoints ) ), cl_logDEBUG1 );
//
//        connection->sendMessage(std::move(response));
//    }
//    else
//    {
//        NX_LOGX( lit("Client %1 connects to %2, error: unknown host")
//                .arg( QString::fromUtf8( userName ) )
//                .arg( QString::fromUtf8( hostName ) ), cl_logDEBUG1 );
//
//        sendErrorResponse( connection, message.header, stun::cc::error::notFound,
//            "Unknown host: " + hostName );
//    }
//}

//void PeerRegistrator::connectionResult(
//    const ConnectionStrongRef& /*connection*/,
//    api::ConnectionResultRequest /*request*/,
//    std::function<void(api::ResultCode)> completionHandler)
//{
//    //TODO #ak
//    completionHandler(api::ResultCode::ok);
//}
//

} // namespace hpm
} // namespace nx
