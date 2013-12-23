/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_server_connection.h"
#include "stun_stream_socket_server.h"


StunServerConnection::StunServerConnection(
    StunStreamSocketServer* socketServer,
    AbstractCommunicatingSocket* sock )
:
    BaseType( socketServer, sock )
{
}

void StunServerConnection::processMessage( const nx_stun::Message& /*request*/, boost::optional<nx_stun::Message>* const /*response*/ )
{
    //TODO/IMPL
}
