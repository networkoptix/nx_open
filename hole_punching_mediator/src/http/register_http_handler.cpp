/**********************************************************
* 5 sep 2014
* a.kolesnikov
***********************************************************/

#include "register_http_handler.h"

#include "http_stream_socket_server.h"


const QString RegisterSystemHttpHandler::HANDLER_PATH = QLatin1String("/register");

bool RegisterSystemHttpHandler::processRequest( const std::weak_ptr<HttpServerConnection>& connection, nx_http::Message&& message )
{
    //RegisteredSystemsDataManager::saveRegistrationDataAsync

    //TODO #ak
    assert( message.type == nx_http::MessageType::request );

    nx_http::Message response( nx_http::MessageType::response );
    response.response->statusLine.version = message.request->requestLine.version;
    response.response->statusLine.statusCode = nx_http::StatusCode::ok;
    response.response->statusLine.reasonPhrase = nx_http::StatusCode::toString( response.response->statusLine.statusCode );

    response.response->messageBody = "<html><h1>Hello, world</h1></html>\n";
    response.response->headers.insert( nx_http::HttpHeader( "Connection", "close" ) );
    response.response->headers.insert( nx_http::HttpHeader( "Content-Type", "text/html" ) );
    response.response->headers.insert( nx_http::HttpHeader( "Content-Size", QByteArray::number( response.response->messageBody.size() ) ) );

    return connection.lock()->sendMessage( std::move( response ), std::bind( &RegisterSystemHttpHandler::responseSent, this, connection ) );
}

void RegisterSystemHttpHandler::responseSent( const std::weak_ptr<HttpServerConnection>& connection )
{
    const std::shared_ptr<HttpServerConnection>& strongConnectionRef = connection.lock();
    if( !strongConnectionRef )
        return;
    strongConnectionRef->closeConnection();
}
