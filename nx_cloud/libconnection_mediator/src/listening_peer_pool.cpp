
#include "listening_peer_pool.h"

#include <iostream>

#include <common/common_globals.h>
#include <nx/utils/log/log.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <nx/network/cloud_connectivity/data/resolve_data.h>


namespace nx {
namespace hpm {

ListeningPeerPool::ListeningPeerPool( AbstractCloudDataProvider* cloudData,
                                      stun::MessageDispatcher* dispatcher )
    : RequestProcessor( cloudData )
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
            stun::cc::methods::resolve,
            [ this ]( const ConnectionSharedPtr& connection, stun::Message message )
                { resolve( connection, std::move( message ) ); } );
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

            NX_LOGX( lit("Peer %2.%3 succesfully bound, endpoints=%4")
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
            NX_LOGX( lit("Peer %1.%2 started to listen")
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

void ListeningPeerPool::resolve(
    const ConnectionSharedPtr& connection,
    stun::Message request)
{
    nx::cc::api::ResolveRequest requestData;
    if (!requestData.parse(request))
        return errorResponse(
            connection,
            request.header,
            stun::error::badRequest,
            requestData.errorText());

    QnMutexLocker lk(&m_mutex);
    const auto peer = m_peers.search(requestData.hostName);
    if (!peer)
    {
        lk.unlock();
        NX_LOGX(lm("Could not resolve host %1. client address: %2")
            .arg(requestData.hostName).arg(connection->socket()->getForeignAddress().toString()),
            cl_logDEBUG2);

        return errorResponse(
            connection,
            request.header,
            stun::cc::error::notFound,
            "Unknown host: " + requestData.hostName);
    }

    stun::Message response(stun::Header(
        stun::MessageClass::successResponse,
        request.header.method,
        std::move(request.header.transactionId)));

    nx::cc::api::ResolveResponse responseData;

    if (!peer->endpoints.empty())
        responseData.endpoints = peer->endpoints;
    responseData.connectionMethods = 
        nx::cc::api::ConnectionMethod::udpHolePunching |
        nx::cc::api::ConnectionMethod::proxy;

    responseData.serialize(&response);

    if (!peer->isListening)
    { /* TODO: StunEndpointList */
    }

    NX_LOGX(lm("Successfully resolved host %1 (requested from %2), endpoints=%3")
        .arg(requestData.hostName).arg(connection->socket()->getForeignAddress().toString())
        .arg(containerString(peer->endpoints)),
        cl_logDEBUG2);

    lk.unlock();
    connection->sendMessage(std::move(response));
}

void ListeningPeerPool::connect( const ConnectionSharedPtr& connection,
                                 stun::Message message )
{
    const auto userNameAttr = message.getAttribute< stun::cc::attrs::PeerId >();
    if( !userNameAttr )
        return errorResponse( connection, message.header, stun::error::badRequest,
            "Attribute ClientId is required" );

    const auto hostNameAttr = message.getAttribute< stun::cc::attrs::HostName >();
    if( !hostNameAttr )
        return errorResponse( connection, message.header, stun::error::badRequest,
            "Attribute HostName is required" );

    const auto userName = userNameAttr->getString();
    const auto hostName = hostNameAttr->getString();

    QnMutexLocker lk( &m_mutex );
    if( const auto peer = m_peers.search( hostName ) )
    {
        stun::Message response( stun::Header(
            stun::MessageClass::successResponse, message.header.method,
            std::move( message.header.transactionId ) ) );

        if( !peer->endpoints.empty() )
            response.newAttribute< stun::cc::attrs::PublicEndpointList >( peer->endpoints );

        if( !peer->isListening )
            { /* TODO: StunEndpointList */ }

        NX_LOGX( lit("Client %1 connects to %2, endpoints=%3")
                .arg( QString::fromUtf8( userName ) )
                .arg( QString::fromUtf8( hostName ) )
                .arg( containerString( peer->endpoints ) ), cl_logDEBUG1 );

        connection->sendMessage( std::move( response ) );
    }
    else
    {
        NX_LOGX( lit("Client %1 connects to %2, error: unknown host")
                .arg( QString::fromUtf8( userName ) )
                .arg( QString::fromUtf8( hostName ) ), cl_logDEBUG1 );

        errorResponse( connection, message.header, stun::cc::error::notFound,
            "Unknown host: " + hostName );
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
            NX_LOGX( lit("Peer %2.%3 has been disconnected")
                    .arg( QString::fromUtf8( systemIt->first ) )
                    .arg( QString::fromUtf8( serverIt->first ) ), cl_logDEBUG1 );

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
        else
            return serverIt->second;
    }

    //return systemIt->second.begin()->second;
    return boost::none; //forbidding auto-server resolve by system name for now to avoid auto-reconnection to another server
}

} // namespace hpm
} // namespace nx
