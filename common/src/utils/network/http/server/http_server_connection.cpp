/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#include "http_server_connection.h"

#include <memory>

#include "http_message_dispatcher.h"
#include "http_stream_socket_server.h"


HttpServerConnection::HttpServerConnection(
    nx_http::HttpStreamSocketServer* socketServer,
    AbstractCommunicatingSocket* sock )
:
    BaseType( socketServer, sock )
{
}

HttpServerConnection::~HttpServerConnection()
{
}

void HttpServerConnection::processMessage( nx_http::Message&& request )
{
    if( !nx_http::MessageDispatcher::instance()->dispatchRequest( this, std::move(request) ) )
    {
        //creating and sending error response
        nx_http::Message response( nx_http::MessageType::response );
        response.response->statusLine.version = request.request->requestLine.version;
        response.response->statusLine.statusCode = nx_http::StatusCode::notFound;
        response.response->statusLine.reasonPhrase = nx_http::StatusCode::toString( response.response->statusLine.statusCode );
        sendMessage( std::move( response ), std::bind( &HttpServerConnection::closeConnection, this ) );
    }
}
