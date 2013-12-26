/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#include "http_server_connection.h"

#include "http_stream_socket_server.h"


HttpServerConnection::HttpServerConnection(
    nx_http::HttpStreamSocketServer* socketServer,
    AbstractCommunicatingSocket* sock )
:
    BaseType( socketServer, sock )
{
}

void HttpServerConnection::processMessage( const nx_http::HttpMessage& request )
{
    //TODO/IMPL
}
