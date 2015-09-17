#include "listening_peer_pool.h"

#include <common/common_globals.h>
#include <utils/common/log.h>
#include <utils/network/stun/cc/custom_stun.h>

namespace nx {
namespace hpm {

ListeningPeerPool::ListeningPeerPool( stun::MessageDispatcher* dispatcher )
{
    using namespace std::placeholders;
    const auto result =
        dispatcher->registerRequestProcessor(
            stun::cc::methods::bind,
            [ this ]( const ConnectionSharedPtr& connection, stun::Message message )
                { bind( connection, std::move( message ) ); } ) &&
        dispatcher->registerRequestProcessor(
            stun::cc::methods::listen,
            [ this ]( const ConnectionSharedPtr& connection, stun::Message message )
                { listen( connection, std::move( message ) ); } ) &&
        dispatcher->registerRequestProcessor(
            stun::cc::methods::connect,
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
            if( const auto attr =
                    message.getAttribute< stun::cc::attrs::PublicEndpointList >() )
                peer->endpoints = attr->get();
            else
                peer->endpoints.clear();

            NX_LOG( lit("%1 Peer %2/%3 succesfully bound, endpoints=%4")
                    .arg( Q_FUNC_INFO )
                    .arg( QString::fromUtf8( mediaserver->systemId ) )
                    .arg( QString::fromUtf8( mediaserver->serverId ) )
                    .arg( containerString( peer->endpoints ) ), cl_logDEBUG1 );

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
            NX_LOG( lit("%1 Peer %2/%3 started to listen")
                    .arg( Q_FUNC_INFO )
                    .arg( QString::fromUtf8( mediaserver->systemId ) )
                    .arg( QString::fromUtf8( mediaserver->serverId ) ),
                    cl_logDEBUG1 );

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
    const auto userNameAttr = message.getAttribute< stun::cc::attrs::ClientId >();
    if( !userNameAttr || userNameAttr->value.isEmpty() )
        return errorResponse( connection, message.header, stun::error::badRequest,
            "Attribute ClientId is required" );

    const auto hostNameAttr = message.getAttribute< stun::cc::attrs::HostName >();
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
            response.newAttribute< stun::cc::attrs::PublicEndpointList >( peer->endpoints );

        if( !peer->isListening )
            { /* TODO: StunEndpointList */ }

        NX_LOG( lit("%1 Client %2 connects to %3, endpoints=%4").arg( Q_FUNC_INFO )
                .arg( QString::fromUtf8( userNameAttr->value ) )
                .arg( QString::fromUtf8( hostNameAttr->value ) ), cl_logDEBUG2 );

        connection->sendMessage( std::move( response ) );
    }
    else
    {
        errorResponse( connection, message.header, stun::cc::error::notFound,
            "Can not find a server " + hostNameAttr->value );
    }
}

ListeningPeerPool::MediaserverPeer::MediaserverPeer( ConnectionWeakPtr connection_ )
    : connection( std::move( connection_ ) )
    , isListening( false )
{
}

ListeningPeerPool::MediaserverPeer::MediaserverPeer( MediaserverPeer&& peer )
    : connection( std::move( peer.connection ) )
    , isListening( peer.isListening )
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
        connection.lock()->registerCloseHandler( [ = ]()
        {
            NX_LOG( lit("%1 Peer %2/%3 has been desconnected")
                    .arg( Q_FUNC_INFO )
                    .arg( QString::fromUtf8( systemIt->first ) )
                    .arg( QString::fromUtf8( serverIt->first ) ), cl_logDEBUG2 );

            QnMutexLocker lk( mutex );
            systemIt->second.erase( serverIt );
            if( systemIt->second.empty() )
                m_peers.erase( systemIt );
        } );

        return serverIt->second;
    }

    return serverIt->second;
}

boost::optional< ListeningPeerPool::MediaserverPeer& >
    ListeningPeerPool::SystemPeers::search( const String& hostName )
{
    // TODO: hostName alias resolution in cloud_db

    const auto ids = hostName.split( '.' );
    if( ids.size() > 2 )
        return boost::none;

    const auto systemIt = m_peers.find( ids.last() );
    if( systemIt == m_peers.end() )
        return boost::none;

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
