#include "listening_peer_pool.h"

#include "stun/custom_stun.h"
#include "mediaserver_api.h"

namespace nx {
namespace hpm {

ListeningPeerPool::ListeningPeerPool( stun::MessageDispatcher* dispatcher )
{
    using namespace std::placeholders;
    const auto result =
        dispatcher->registerRequestProcessor(
            methods::bind,
            [ this ]( const ConnectionSharedPtr& connection, stun::Message message )
                { bind( connection, std::move( message ) ); } ) &&
        dispatcher->registerRequestProcessor(
            methods::listen,
            [ this ]( const ConnectionSharedPtr& connection, stun::Message message )
                { listen( connection, std::move( message ) ); } ) &&
        dispatcher->registerRequestProcessor(
            methods::connect,
            [ this ]( const ConnectionSharedPtr& connection, stun::Message message )
                { connect( connection, std::move( message ) ); } );

    // TODO: NX_LOG
    Q_ASSERT_X( result, Q_FUNC_INFO, "Could not register one of processors" );
}

void ListeningPeerPool::bind( const ConnectionSharedPtr& connection,
                              stun::Message message )
{
    if( const auto mediaserver = getMediaserverData( connection, message ) )
    {
        QnMutexLocker lk( &m_mutex );
        if( const auto peer = m_peers.peer( connection, *mediaserver, &m_mutex ) )
        {
            if( const auto attr = message.getAttribute< attrs::PublicEndpointList >() )
                peer->endpoints = attr->get();
            else
                peer->endpoints.clear();

            successResponse( connection, message.header );
        }
        else
        {
            errorResponse( connection, message.header, stun::error::badRequest,
                "This mediaserver is already bound from diferent connection" );
        }
    }
}

void ListeningPeerPool::listen( const ConnectionSharedPtr& connection,
                                stun::Message message )
{
    if( const auto mediaserver = getMediaserverData( connection, message ) )
    {
        QnMutexLocker lk( &m_mutex );
        if( const auto peer = m_peers.peer( connection, *mediaserver, &m_mutex ) )
        {
            // TODO: configure StunEndpointList
            peer->isListening = true;
            successResponse( connection, message.header );
        }
        else
        {
            errorResponse( connection, message.header, stun::error::badRequest,
                "This mediaserver is already bound from diferent connection" );
        }
    }
}

void ListeningPeerPool::connect( const ConnectionSharedPtr& connection,
                                 stun::Message message )
{
    const auto userNameAttr = message.getAttribute< attrs::UserName >();
    if( !userNameAttr || userNameAttr->value.isEmpty() )
        return errorResponse( connection, message.header, stun::error::badRequest,
            "Attribute UserName is required" );

    const auto hostNameAttr = message.getAttribute< attrs::HostName >();
    if( !hostNameAttr || hostNameAttr->value.isEmpty() )
        return errorResponse( connection, message.header, stun::error::badRequest,
            "Attribute HostName is required" );

    QnMutexLocker lk( &m_mutex );
    if( const auto peer = m_peers.search( hostNameAttr->value ) )
    {
        stun::Message response( stun::Header(
            stun::MessageClass::successResponse, message.header.method,
            std::move( message.header.transactionId ) ) );

        if( !peer->endpoints.empty() )
            response.newAttribute< attrs::PublicEndpointList >( peer->endpoints );

        if( !peer->isListening )
            { /* TODO: StunEndpointList */ }

        connection->sendMessage( std::move( response ) );
    }
    else
    {
        errorResponse( connection, message.header, error::notFound,
            "Can not find a server " + hostNameAttr->value );
    }
}

ListeningPeerPool::MediaserverPeer::MediaserverPeer( ConnectionWeakPtr connection_ )
    : connection( std::move( connection_) )
    , isListening( false )
{
}

boost::optional< ListeningPeerPool::MediaserverPeer& >
    ListeningPeerPool::SystemPeers::peer(
        ConnectionWeakPtr connection,
        const RequestProcessor::MediaserverData& mediaserver,
        QnMutex* mutex )
{
    const auto systemInsert = m_peers.emplace(
                mediaserver.systemId, std::map< String, MediaserverPeer >() );

    const auto& systemIt = systemInsert.first;
    const auto serverInsert = systemIt->second.emplace(
                mediaserver.serverId, connection );

    const auto& serverIt = serverInsert.first;
    if( serverInsert.second )
    {
        connection->registerCloseHandler( [ = ]()
        {
            QnMutexLocker lk( mutex );
            systemIt->second.erase( serverIt );
            if( systemIt->second.empty() )
                m_peers.erase( systemIt );
        } );

        return serverIt->second;
    }

    if( serverIt->second.connection != connection )
        return boost::none;

    return serverIt->second;
}

boost::optional< ListeningPeerPool::MediaserverPeer& >
    ListeningPeerPool::SystemPeers::search( String hostName )
{
    const auto ids = hostName.split( '.' );
    if( ids.size() > 2 )
        return boost::none;

    const auto systemIt = m_peers.find( ids.last() );
    if( ids.size() == 2 )
    {
        const auto serverIt = systemIt->second.find( ids.first() );
        if( serverIt == systemIt->second.end() )
            return boost::none;
    }

    // TODO: select the best server, not the first one
    return systemIt->second.begin()->second;
}

} // namespace hpm
} // namespace nx
