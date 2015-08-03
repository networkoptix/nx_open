
#include "listening_peer_pool.h"

#include "stun/custom_stun.h"
#include "stun/stun_message_dispatcher.h"

#include <atomic>

namespace nx {
namespace hpm {

static std::atomic<ListeningPeerPool*> ListeningPeerPool_instance;

ListeningPeerPool::ListeningPeerPool()
{
    assert( ListeningPeerPool_instance.load() == nullptr );
    ListeningPeerPool_instance.store( this, std::memory_order_relaxed );
}

ListeningPeerPool::~ListeningPeerPool()
{
    assert( ListeningPeerPool_instance.load() == this );
    ListeningPeerPool_instance.store( nullptr, std::memory_order_relaxed );
}

bool ListeningPeerPool::registerRequestProcessors( STUNMessageDispatcher& dispatcher )
{
    using namespace std::placeholders;
    return
        dispatcher.registerRequestProcessor(
            methods::PING, std::bind( &ListeningPeerPool::ping, this, _1, _2 ) ) &&
        dispatcher.registerRequestProcessor(
            methods::LISTEN, std::bind( &ListeningPeerPool::listen, this, _1, _2 ) ) &&
        dispatcher.registerRequestProcessor(
            methods::CONNECT, std::bind( &ListeningPeerPool::connect, this, _1, _2 ) );
}

bool ListeningPeerPool::ping( StunServerConnection* connection, stun::Message&& message )
{
    const auto systemAttr = message.getAttribute< attrs::SystemId >();
    if( !systemAttr )
        return errorResponse( connection, message, stun::error::BAD_REQUEST,
                              "Attribute SystemId is required" );

    const auto serverAttr = message.getAttribute< attrs::ServerId >();
    if( !serverAttr )
        return errorResponse( connection, message, stun::error::BAD_REQUEST,
                              "Attribute ServerId is required" );

    const auto authAttr = message.getAttribute< attrs::Authorization >();
    if( !authAttr )
        return errorResponse( connection, message, stun::error::BAD_REQUEST,
                              "Attribute Authorization is required" );

    // TODO: check for authorization

    const auto endpointsAttr = message.getAttribute< attrs::PublicEndpointList >();
    if( !endpointsAttr )
        return errorResponse( connection, message, stun::error::BAD_REQUEST,
                              "Attribute PublicEndpointList is required" );

    // TODO: ping endpoints and remove unaccesible
    std::list< SocketAddress > endpoints = endpointsAttr->get();

    stun::Message response( stun::Header(
        stun::MessageClass::successResponse, message.header.method,
        std::move( message.header.transactionId ) ) );

    response.newAttribute< attrs::PublicEndpointList >( endpoints );
    return connection->sendMessage( std::move( response ) );
}

bool ListeningPeerPool::listen( StunServerConnection* /*connection*/, stun::Message&& /*message*/ )
{
    //retrieving requests parameters:
        //peer id
        //address to listen on. This address MUST have following format: {server_guid}.{system_name}
        //registration_key. It is variable-length string

    //checking that system_name has been registered previously with supplied registration_key by invoking RegisteredDomainsDataManager::getDomainData

    //checking that address {server_guid}.{system_name} is not occupied already

    //binding to address, sending success response
        //NOTE single server is allowed to listen on multiple mediators

    return false;
}

bool ListeningPeerPool::connect( StunServerConnection* /*connection*/, stun::Message&& /*message*/ )
{
    //retrieving requests parameters:
        //address to connect to. This address has following format: {server_guid}.{system_name} or just {system_name}. 
            //At the latter case connecting to binded server, closest to the client

    //checking if requested address is bound to

    //if just system name supplied and there are multiple servers with requested system_name listening, selecting server

    //if selected server is listening on another mediator, re-routing client to that mediator

    //sending connection_requested indication containing client address to the selected server

    //sending success response with selected server address

    return false;
}

ListeningPeerPool* ListeningPeerPool::instance()
{
    return ListeningPeerPool_instance.load( std::memory_order_relaxed );
}

bool ListeningPeerPool::errorResponse(
        StunServerConnection* connection, stun::Message& request, int code, String reason )
{
    stun::Message response( stun::Header(
        stun::MessageClass::errorResponse, request.header.method,
        std::move( request.header.transactionId ) ) );

    response.newAttribute< stun::attrs::ErrorDescription >( code, std::move(reason) );
    return connection->sendMessage( std::move( response ) );
}

} // namespace hpm
} // namespace nx
