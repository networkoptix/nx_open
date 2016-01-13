/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#include "cloud_connect_processor.h"

#include <nx/network/stun/message_dispatcher.h>

#include "listening_peer_pool.h"


namespace nx {
namespace hpm {

CloudConnectProcessor::CloudConnectProcessor(
    AbstractCloudDataProvider* cloudData,
    nx::stun::MessageDispatcher* dispatcher,
    ListeningPeerPool* const listeningPeerPool)
:
    RequestProcessor(cloudData),
    m_listeningPeerPool(listeningPeerPool)
{
    dispatcher->registerRequestProcessor(
        stun::cc::methods::connect,
        [this](const ConnectionStrongRef& connection, stun::Message message)
            { connect( std::move(connection), std::move( message ) ); } );

    dispatcher->registerRequestProcessor(
        stun::cc::methods::connectionResult,
        [this](const ConnectionStrongRef& connection, stun::Message message)
        {
            processRequestWithNoOutput(
                &CloudConnectProcessor::connectionResult,
                this,
                std::move(connection),
                std::move(message));
        });
}

void CloudConnectProcessor::connect(
    const ConnectionStrongRef& connection,
    stun::Message message )
{
    //TODO #ak
    sendErrorResponse(
        connection,
        message.header, stun::cc::error::notFound,
        "Not implemented");

    
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
}

void CloudConnectProcessor::connectionResult(
    const ConnectionStrongRef& /*connection*/,
    api::ConnectionResultRequest /*request*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::ok);
}


} // namespace hpm
} // namespace nx
