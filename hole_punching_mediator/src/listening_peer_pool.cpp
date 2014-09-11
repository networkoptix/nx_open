
#include "listening_peer_pool.h"

#include <atomic>


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

bool ListeningPeerPool::processBindRequest( const std::weak_ptr<StunServerConnection>& connection, nx_stun::Message&& message )
{
    //retrieving requests parameters:
        //address to bind to. This address MUST have following format: {server_guid}.{system_name}
        //registration_key. It is variable-length string

    //checking that system_name has been registered previously with supplied registration_key by invoking RegisteredDomainsDataManager::getDomainData

    //checking that address {server_guid}.{system_name} is not occupied already

    //binding to address, sending success response
        //NOTE single server is allowed to listen on multiple mediators

    return false;
}

bool ListeningPeerPool::processConnectRequest( const std::weak_ptr<StunServerConnection>& connection, nx_stun::Message&& message )
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
