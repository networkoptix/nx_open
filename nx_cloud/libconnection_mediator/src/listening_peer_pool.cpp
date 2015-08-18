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
            methods::listen,
            [this]( stun::ServerConnection* connection, stun::Message message ) {
                listen( connection, std::move( message ) );
            } ) &&
        dispatcher->registerRequestProcessor(
            methods::connect,
            [this]( stun::ServerConnection* connection, stun::Message message ) {
                connect( connection, std::move( message ) );
            } );

    // TODO: NX_LOG
    Q_ASSERT_X( result, Q_FUNC_INFO, "Could not register one of processors" );
}

void ListeningPeerPool::listen( stun::ServerConnection* connection,
                                stun::Message message )
{
    //retrieving requests parameters:
        //peer id
        //address to listen on. This address MUST have following format: {server_guid}.{system_name}
        //registration_key. It is variable-length string

    //checking that system_name has been registered previously with supplied registration_key by invoking RegisteredDomainsDataManager::getDomainData

    //checking that address {server_guid}.{system_name} is not occupied already

    //binding to address, sending success response
        //NOTE single server is allowed to listen on multiple mediators

    errorResponse( connection, message.header, stun::error::serverError,
                   "Method is not implemented yet" );
}

void ListeningPeerPool::connect( stun::ServerConnection* connection,
                                 stun::Message message )
{
    //retrieving requests parameters:
        //address to connect to. This address has following format: {server_guid}.{system_name} or just {system_name}. 
            //At the latter case connecting to binded server, closest to the client

    //checking if requested address is bound to

    //if just system name supplied and there are multiple servers with requested system_name listening, selecting server

    //if selected server is listening on another mediator, re-routing client to that mediator

    //sending connection_requested indication containing client address to the selected server

    //sending success response with selected server address

    errorResponse( connection, message.header, stun::error::serverError,
                   "Method is not implemented yet" );
}

} // namespace hpm
} // namespace nx
