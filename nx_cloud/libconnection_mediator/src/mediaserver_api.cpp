#include "mediaserver_api.h"

#include <api/model/ping_reply.h>
#include <rest/server/json_rest_result.h>
#include <nx/utils/log/log.h>

#include <QJsonDocument>

namespace nx {
namespace hpm {

MediaserverApiBase::MediaserverApiBase( AbstractCloudDataProvider* cloudData,
                                    stun::MessageDispatcher* dispatcher )
    : RequestProcessor( cloudData )
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

struct PingCollector
{
    QnMutex mutex;
    size_t expected;
    std::list< SocketAddress > endpoints;
    std::weak_ptr< stun::ServerConnection > connection;

    PingCollector( size_t expected_,
                   const std::weak_ptr< stun::ServerConnection >& connection_ )
        : expected( expected_ )
        , connection( connection_ )
    {}
};

void MediaserverApiBase::ping( const ConnectionSharedPtr& connection,
                             stun::Message message )
{
    if( const auto mediaserver = getMediaserverData( connection, message ) )
    {
        const auto endpointsAttr =
                message.getAttribute< stun::cc::attrs::PublicEndpointList >();
        if( !endpointsAttr )
        {
            sendErrorResponse( connection, message.header, stun::error::badRequest,
                           "Attribute PublicEndpointList is required" );
            return;
        }

        const auto endpoints = endpointsAttr->get();
        const auto collector = std::make_shared< PingCollector >( endpoints.size(), connection );
        const auto& method = message.header.method;
        const auto& transactionId = message.header.transactionId;
        const auto onPinged = [ = ]( SocketAddress address, bool result )
        {
            QnMutexLocker lk( &collector->mutex );
            if( result )
                collector->endpoints.push_back( std::move( address ) );

            if( --collector->expected )
                return; // wait for others...

            if( auto connection = collector->connection.lock() )
            {
                NX_LOGX( lit("Peer %1.%2 succesfully pinged %3")
                        .arg( QString::fromUtf8( mediaserver->systemId ) )
                        .arg( QString::fromUtf8( mediaserver->serverId ) )
                        .arg( containerString( collector->endpoints ) ), cl_logDEBUG1 );

                stun::Message response( stun::Header(
                    stun::MessageClass::successResponse, method,
                    std::move( transactionId ) ) );

                response.newAttribute< stun::cc::attrs::PublicEndpointList >(
                            collector->endpoints );

                connection->sendMessage( std::move( response ) );
            }
        };

        for( auto& ep : endpoints )
            pingServer( ep, mediaserver->serverId, onPinged );
    }
}

// impl

MediaserverApi::MediaserverApi( AbstractCloudDataProvider* cloudData,
                                stun::MessageDispatcher* dispatcher )
    : MediaserverApiBase( cloudData, dispatcher )
{
}

static QString scanResponseForErrors( const nx::Buffer& buffer, const String& expectedId )
{
    QnJsonRestResult result;
    if( !QJson::deserialize( buffer, &result ) )
        return lit("bad REST result: %3").arg( QString::fromUtf8( buffer ) );

    QnPingReply reply;
    if( !QJson::deserialize( result.reply, &reply ) )
        return lit("bad REST reply: %3").arg( result.reply.toString() );

    if( reply.moduleGuid.toSimpleString().toUtf8() != expectedId )
        return lit("moduleGuid '%3' doesnt match expectedId '%4'")
                .arg( reply.moduleGuid.toString() )
                .arg( QString::fromUtf8( expectedId ) );

    return QString();
}

void MediaserverApi::pingServer( const SocketAddress& address, const String& expectedId,
                                 std::function< void( SocketAddress, bool ) > onPinged )
{
    auto httpClient = nx_http::AsyncHttpClient::create();
    QObject::connect( httpClient.get(), &nx_http::AsyncHttpClient::done,
                      [ = ]( nx_http::AsyncHttpClientPtr httpClient )
        {
            {
                QnMutexLocker lk( &m_mutex );
                m_httpClients.erase( httpClient );
            }

            if( !httpClient->hasRequestSuccesed() )
            {
                NX_LOGX( lit("Response from %1 has failed")
                         .arg( address.toString() ), cl_logDEBUG1 );
                return onPinged( std::move( address ), false );
            }

            const auto buffer = httpClient->fetchMessageBodyBuffer();
            const auto error = scanResponseForErrors( buffer, expectedId );
            if( !error.isEmpty() )
            {
                NX_LOGX( lit("Response from %1 has error: %2")
                         .arg( address.toString() ).arg( error ), cl_logDEBUG1 );
                return onPinged( std::move( address ), false );
            }
            onPinged( std::move( address ), true );
        } );

    httpClient->doGet( lit( "http://%1/api/ping" ).arg( address.toString() ) );

    QnMutexLocker lk( &m_mutex );
    m_httpClients.insert( std::move( httpClient ) );
}

} // namespace hpm
} // namespace nx
