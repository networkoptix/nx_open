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

HttpServerConnection::~HttpServerConnection()
{
}

void HttpServerConnection::processMessage( nx_http::Message&& request )
{
    //TODO #ak

    nx_http::Message response( nx_http::MessageType::response );
    response.response->statusLine.version = request.request->requestLine.version;
    response.response->statusLine.statusCode = nx_http::StatusCode::ok;
    response.response->statusLine.reasonPhrase = nx_http::StatusCode::toString( response.response->statusLine.statusCode );

    response.response->messageBody = "<html><h1>Hello, world</h1></html>\n";
    response.response->headers.insert( nx_http::HttpHeader( "Connection", "close" ) );
    response.response->headers.insert( nx_http::HttpHeader( "Content-Type", "text/html" ) );
    response.response->headers.insert( nx_http::HttpHeader( "Content-Size", QByteArray::number( response.response->messageBody.size() ) ) );

    sendMessage( std::move( response ), std::bind( &HttpServerConnection::responseSent, this ) );
}

void HttpServerConnection::responseSent()
{
    closeConnection();
}
