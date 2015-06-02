/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#include "http_server_connection.h"

#include <memory>

#include <utils/common/util.h>

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
        //TODO incoming message body
        //    should use AbstractMsgBodySource also

        //TODO #ak interleaving support
        //    can add sequence to sendResponseFunc and use it to queue responses

        using namespace std::placeholders;

        //TODO #ak performing authentication
        //    authentication manager should be able to return some custom data
        //    which will be forwarded to the request handler.
        //    Should this data be template parameter?
        header::WWWAuthenticate wwwAuthenticate;
        if( !nx_http::ServerManagers::instance()->authenticationManager()->authenticate(
                *this,
                *request.request,
                &wwwAuthenticate ) )
        {
            nx_http::Message response( nx_http::MessageType::response );
            response.response->statusLine.statusCode = nx_http::StatusCode::unauthorized;
            response.response->headers.emplace(
                header::WWWAuthenticate::NAME,
                wwwAuthenticate.serialized() );
            return prepareAndSendResponse( std::move( response ), nullptr );
        }

        auto sendResponseFunc = [this](
            nx_http::Message&& response,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody )
        {
            prepareAndSendResponse(
                std::move(response),
                std::move(responseMsgBody) );
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
            response.response->statusLine.statusCode = nx_http::StatusCode::unauthorized;
            return prepareAndSendResponse( std::move( response ), nullptr );
        }
    }

    void HttpServerConnection::prepareAndSendResponse(
        nx_http::Message&& msg,
        std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody )
    {
        //msg.response->statusLine.version = request.request->requestLine.version;
        msg.response->statusLine.reasonPhrase =
            nx_http::StatusCode::toString( msg.response->statusLine.statusCode );

        //TODO #ak adding necessary headers
        nx_http::insertOrReplaceHeader(
            &msg.response->headers,
            nx_http::HttpHeader( "Server", nx_http::serverString() ) );
        nx_http::insertOrReplaceHeader(
            &msg.response->headers,
            nx_http::HttpHeader( "Date", dateTimeToHTTPFormat( QDateTime::currentDateTime() ) ) );

        //TODO #ak connection persistency

        m_currentMsgBody = std::move(responseMsgBody);
        sendMessage(
            std::move( msg ),
            std::bind( &HttpServerConnection::responseSent, this ) );
    }

    void HttpServerConnection::responseSent()
    {
        //TODO #ak sending message body (m_currentMsgBody)
        closeConnection();
    }
}
