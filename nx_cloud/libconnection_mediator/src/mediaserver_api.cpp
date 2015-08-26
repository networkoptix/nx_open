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
            methods::ping, 
            [this]( stun::ServerConnection* connection, stun::Message message ) {
                ping( connection, std::move( message ) );
        } );

    // TODO: NX_LOG
    Q_ASSERT_X( result, Q_FUNC_INFO, "Could not register ping processor" );
}

void MediaserverApiIf::ping( stun::ServerConnection* connection,
                             stun::Message message )
{
    if( const auto mediaserver = getMediaserverData( connection, message ) )
    {
        const auto endpointsAttr = message.getAttribute< attrs::PublicEndpointList >();
        if( !endpointsAttr )
        {
            errorResponse( connection, message.header, stun::error::badRequest,
                           "Attribute PublicEndpointList is required" );
            return;
        }

        std::list< SocketAddress > endpoints = endpointsAttr->get();
        endpoints.remove_if( [ & ]( const SocketAddress& addr )
                             { return !pingServer( addr, mediaserver->serverId ); } );

        stun::Message response( stun::Header(
            stun::MessageClass::successResponse, message.header.method,
            std::move( message.header.transactionId ) ) );

        response.newAttribute< attrs::PublicEndpointList >( endpoints );
        connection->sendMessage( std::move( response ) );
    }
}

// impl

MediaserverApi::MediaserverApi( stun::MessageDispatcher* dispatcher )
    : MediaserverApiIf( dispatcher )
{
}

bool MediaserverApi::pingServer( const SocketAddress& address, const QnUuid& expectedId )
{
    QUrl url( address.address.toString() );
    url.setPort( address.port );
    url.setPath( lit("/api/ping") );

    nx_http::HttpClient client;
    if( !client.doGet( url ) )
    {
        NX_LOG( lit("%1 Url %2 is unaccesible")
                .arg( Q_FUNC_INFO ).arg( url.toString() ), cl_logDEBUG1 )
        return false;
    }

    const auto buffer = client.fetchMessageBodyBuffer();

    QnJsonRestResult result;
    if( !QJson::deserialize( buffer, &result ) )
    {
        NX_LOG( lit("%1 Url %2 response is a bad REST result: %3")
                .arg( Q_FUNC_INFO ).arg( url.toString() )
                .arg( QString::fromUtf8( buffer ) ), cl_logDEBUG1 );
        return false;
    }

    QnPingReply reply;
    if( !QJson::deserialize( result.reply, &reply ) )
    {
        NX_LOG( lit("%1 Url %2 response is a bad REST result: %3")
                .arg( Q_FUNC_INFO ).arg( url.toString() )
                .arg( result.reply.toString() ), cl_logDEBUG1 );
        return false;
    }

    if( reply.moduleGuid != expectedId) {
        NX_LOG( lit("%1 Url %2 moduleGuid %3 doesnt match expectedId %4")
                .arg( Q_FUNC_INFO ).arg( url.toString() )
                .arg( reply.moduleGuid.toString() )
                .arg( expectedId.toString() ), cl_logDEBUG1 );
        return false;
    }

    return true;
}

} // namespace hpm
} // namespace nx
