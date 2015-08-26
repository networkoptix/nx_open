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
        std::unique_ptr<AbstractCommunicatingSocket> sock )
    :
        BaseType( socketServer, std::move(sock) ),
        m_isPersistent( false )
    {
    }

    HttpServerConnection::~HttpServerConnection()
    {
    }

    void HttpServerConnection::processMessage( nx_http::Message&& request )
    {
        //TODO incoming message body
        //    should use AbstractMsgBodySource also

        //TODO #ak pipelining support
        //    can add sequence to sendResponseFunc and use it to queue responses

        //checking if connection is persistent
        checkForConnectionPersistency( request );

        //TODO #ak performing authentication
        //    authentication manager should be able to return some custom data
        //    which will be forwarded to the request handler.
        //    Should this data be template parameter?
        stree::ResourceContainer authInfo;
        if( const auto auth = nx_http::ServerManagers::instance()->authenticationManager() )
        {
            header::WWWAuthenticate wwwAuthenticate;
            if( !auth->authenticate(
                    *this,
                    *request.request,
                    &wwwAuthenticate,
                    &authInfo ) )
            {
                nx_http::Message response( nx_http::MessageType::response );
                response.response->statusLine.statusCode = nx_http::StatusCode::unauthorized;
                response.response->headers.emplace(
                    header::WWWAuthenticate::NAME,
                    wwwAuthenticate.serialized() );
                return prepareAndSendResponse( std::move( response ), nullptr );
            }
        }

        auto strongRef = shared_from_this();
        std::weak_ptr<HttpServerConnection> weakThis = strongRef;

        auto sendResponseFunc = [this, weakThis](
            nx_http::Message&& response,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody )
        {
            auto strongThis = weakThis.lock();
            if( !strongThis )
                return; //connection has been removed while request has been processed
            strongThis->prepareAndSendResponse(
                std::move(response),
                std::move(responseMsgBody) );
        };

        MessageDispatcher* const dispatcher = nx_http::ServerManagers::instance()->dispatcher();
        if( !dispatcher ||
            !dispatcher->dispatchRequest(
                *this,
                std::move(request),
                std::move(authInfo),
                std::move(sendResponseFunc) ) )
        {
            //creating and sending error response
            nx_http::Message response( nx_http::MessageType::response );
            response.response->statusLine.statusCode = nx_http::StatusCode::notFound;
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

        if( !responseMsgBody )
            nx_http::insertOrReplaceHeader(
                &msg.response->headers,
                nx_http::HttpHeader( "Content-Length", "0" ) );

        m_currentMsgBody = std::move(responseMsgBody);
        //TODO #ak check sendMessage error code
        sendMessage(
            std::move( msg ),
            std::bind( &HttpServerConnection::responseSent, this ) );
    }

    void HttpServerConnection::responseSent()
    {
        //TODO #ak check sendData error code
        if( !m_currentMsgBody )
        {
            fullMessageHasBeenSent();
            return;
        }

        using namespace std::placeholders;
        if( !m_currentMsgBody->readAsync(
                std::bind( &HttpServerConnection::someMsgBodyRead, this, _1, _2 ) ) )
        {
            closeConnection();
            return;
        }
    }

    void HttpServerConnection::someMsgBodyRead(
        SystemError::ErrorCode errorCode,
        BufferType buf )
    {
        if( errorCode != SystemError::noError )
        {
            closeConnection();
            return;
        }

        if( buf.isEmpty() )
        {
            //done with message body
            fullMessageHasBeenSent();
            return;
        }

        //TODO #ak read and send message body async
        //    move async reading/writing to some separate class (async pipe) to enable reusage

        if( !sendData(
                std::move( buf ),
                std::bind( &HttpServerConnection::someMessageBodySent, this ) ) )
        {
            closeConnection();
            return;
        }
    }

    void HttpServerConnection::someMessageBodySent()
    {
        using namespace std::placeholders;
        if( !m_currentMsgBody->readAsync(
            std::bind( &HttpServerConnection::someMsgBodyRead, this, _1, _2 ) ) )
        {
            closeConnection();
            return;
        }
    }

    void HttpServerConnection::fullMessageHasBeenSent()
    {
        //if connection is NOT persistent then closing it
        if( !m_isPersistent )
            closeConnection();
    }

    void HttpServerConnection::checkForConnectionPersistency( const Message& msg )
    {
        if( msg.type != MessageType::request )
            return;

        const auto& request = *msg.request;

        if( request.requestLine.version == nx_http::http_1_1 )
            m_isPersistent = nx_http::getHeaderValue( request.headers, "Connection" ).toLower() != "close";
        else if( request.requestLine.version == nx_http::http_1_0 )
            m_isPersistent = nx_http::getHeaderValue( request.headers, "Connection" ).toLower() == "keep-alive";
        else    //e.g., RTSP
            m_isPersistent = false;
    }
}
