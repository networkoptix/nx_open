/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#include "http_server_connection.h"

#include <memory>

#include "http_message_dispatcher.h"
#include "http_stream_socket_server.h"
#include "server_managers.h"


namespace nx_http
{
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
        using namespace std::placeholders;

        //TODO #ak performing authentication
        //TODO #ak interleaving support

        auto sendResponseFunc = [this](
            nx_http::Message&& response,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody )
        {
            requestProcessed( std::move(response), std::move( responseMsgBody ) );
        };

        MessageDispatcher* const dispatcher = nx_http::ServerManagers::instance()->dispatcher();
        if( !dispatcher ||
            !dispatcher->dispatchRequest(
                *this,
                std::move(request),
                std::move(sendResponseFunc) ) )
        {
            //creating and sending error response
            nx_http::Message response( nx_http::MessageType::response );
            response.response->statusLine.version = request.request->requestLine.version;
            response.response->statusLine.statusCode = nx_http::StatusCode::notFound;
            response.response->statusLine.reasonPhrase = nx_http::StatusCode::toString( response.response->statusLine.statusCode );
            sendMessage(
                std::move( response ),
                std::bind( &HttpServerConnection::closeConnection, this ) );
        }
    }

    void HttpServerConnection::requestProcessed(
        nx_http::Message&& response,
        std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody )
    {
        sendMessage(
            std::move(response),
            std::bind( &HttpServerConnection::closeConnection, this ) );
        //TODO #ak sending message body
    }
}
