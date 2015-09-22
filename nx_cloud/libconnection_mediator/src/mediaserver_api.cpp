#include "mediaserver_api.h"

#include <api/model/ping_reply.h>
#include <rest/server/json_rest_result.h>
#include <utils/network/http/httpclient.h>
#include <utils/common/log.h>

#include <QJsonDocument>

namespace nx {
namespace hpm {

MediaserverApiIf::MediaserverApiIf( stun::MessageDispatcher* dispatcher )
{
    using namespace std::placeholders;
    const auto result =
        dispatcher->registerRequestProcessor(
            stun::cc::methods::ping,
            [ this ]( const ConnectionSharedPtr& connection, stun::Message message )
                { ping( connection, std::move( message ) ); } );

    // TODO: NX_LOG
    Q_ASSERT_X( result, Q_FUNC_INFO, "Could not register ping processor" );
}

void MediaserverApiIf::ping( const ConnectionSharedPtr& connection,
                             stun::Message message )
{
    if( const auto mediaserver = getMediaserverData( connection, message ) )
    {
        const auto endpointsAttr =
                message.getAttribute< stun::cc::attrs::PublicEndpointList >();
        if( !endpointsAttr )
        {
            errorResponse( connection, message.header, stun::error::badRequest,
                           "Attribute PublicEndpointList is required" );
            return;
        }

        std::list< SocketAddress > endpoints = endpointsAttr->get();
        endpoints.remove_if( [ & ]( const SocketAddress& addr )
            { return !pingServer( addr, mediaserver->serverId ); } );

        NX_LOG( lit("%1 Peer %2/%3 succesfully pinged %4 of %5")
                .arg( Q_FUNC_INFO )
                .arg( QString::fromUtf8( mediaserver->systemId ) )
                .arg( QString::fromUtf8( mediaserver->serverId ) )
                .arg( containerString( endpoints ) )
                .arg( containerString( endpointsAttr->get() ) ), cl_logDEBUG1 );

        stun::Message response( stun::Header(
            stun::MessageClass::successResponse, message.header.method,
            std::move( message.header.transactionId ) ) );

        response.newAttribute< stun::cc::attrs::PublicEndpointList >( endpoints );
        connection->sendMessage( std::move( response ) );
    }
}

// impl

MediaserverApi::MediaserverApi( stun::MessageDispatcher* dispatcher )
    : MediaserverApiIf( dispatcher )
{
}

bool MediaserverApi::pingServer( const SocketAddress& address, const String& expectedId )
{
    // TODO: Async implementation
    const auto url = lit( "http://%1/api/ping" ).arg( address.toString() );
    nx_http::HttpClient client;
    if( !client.doGet( url ) )
    {
        NX_LOG( lit("%1 Url %2 is unaccesible")
                .arg( Q_FUNC_INFO ).arg( url ), cl_logDEBUG1 )
        return false;
    }

    const auto buffer = client.fetchMessageBodyBuffer();

    QnJsonRestResult result;
    if( !QJson::deserialize( buffer, &result ) )
    {
        NX_LOG( lit("%1 Url %2 response is a bad REST result: %3")
                .arg( Q_FUNC_INFO ).arg( url )
                .arg( QString::fromUtf8( buffer ) ), cl_logDEBUG1 );
        return false;
    }

    QnPingReply reply;
    if( !QJson::deserialize( result.reply, &reply ) )
    {
        NX_LOG( lit("%1 Url %2 response is a bad REST result: %3")
                .arg( Q_FUNC_INFO ).arg( url )
                .arg( result.reply.toString() ), cl_logDEBUG1 );
        return false;
    }

    if( reply.moduleGuid.toSimpleString().toUtf8() != expectedId ) {
        NX_LOG( lit("%1 Url %2 moduleGuid '%3' doesnt match expectedId '%4'")
                .arg( Q_FUNC_INFO ).arg( url )
                .arg( reply.moduleGuid.toString() )
                .arg( QString::fromUtf8( expectedId ) ), cl_logDEBUG1 );
        return false;
    }

    return true;
}

} // namespace hpm
} // namespace nx
