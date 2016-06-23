
#include "http_server_connection.h"

#include <memory>

#include <QtCore/QDateTime>

#include <utils/common/util.h>

#include "http_message_dispatcher.h"
#include "http_stream_socket_server.h"


namespace nx_http
{
    HttpServerConnection::HttpServerConnection(
        StreamConnectionHolder<HttpServerConnection>* socketServer,
        std::unique_ptr<AbstractCommunicatingSocket> sock,
        nx_http::AbstractAuthenticationManager* const authenticationManager,
        nx_http::MessageDispatcher* const httpMessageDispatcher )
    :
        BaseType( socketServer, std::move(sock) ),
        m_authenticationManager( authenticationManager ),
        m_httpMessageDispatcher( httpMessageDispatcher ),
        m_isPersistent( false )
    {
    }

    HttpServerConnection::~HttpServerConnection()
    {
    }

    void HttpServerConnection::pleaseStop(
        nx::utils::MoveOnlyFunc<void()> completionHandler)
    {
        BaseType::pleaseStop(
            [this, completionHandler = std::move(completionHandler)]()
            {
                m_currentMsgBody.reset();   //we are in aio thread, so this is ok
                completionHandler();
            });
    }

    void HttpServerConnection::processMessage( nx_http::Message&& requestMessage )
    {
        //TODO incoming message body
        //    should use AbstractMsgBodySource also

        //TODO #ak pipelining support
        //    can add sequence to sendResponseFunc and use it to queue responses

        //checking if connection is persistent
        checkForConnectionPersistency(requestMessage);

        //TODO #ak performing authentication
        //    authentication manager should be able to return some custom data
        //    which will be forwarded to the request handler.
        //    Should this data be template parameter?
        if (!m_authenticationManager)
        {
            onAuthenticationDone(
                true,
                stree::ResourceContainer(),
                std::move(requestMessage),
                boost::none,
                nx_http::HttpHeaders(),
                nullptr);
            return;
        }

        const nx_http::Request& request = *requestMessage.request;
        auto strongRef = shared_from_this();
        std::weak_ptr<HttpServerConnection> weakThis = strongRef;
        m_authenticationManager->authenticate(
            *this,
            request,
            [this, weakThis = std::move(weakThis), 
                requestMessage = std::move(requestMessage)](
                    bool authenticationResult,
                    stree::ResourceContainer authInfo,
                    boost::optional<header::WWWAuthenticate> wwwAuthenticate,
                    nx_http::HttpHeaders responseHeaders,
                    std::unique_ptr<AbstractMsgBodySource> msgBody) mutable
            {
                auto strongThis = weakThis.lock();
                if (!strongThis)
                    return; //connection has been removed while request authentication in progress

                strongThis->post(
                    [this,
                        authenticationResult,
                        authInfo = std::move(authInfo),
                        requestMessage = std::move(requestMessage),
                        wwwAuthenticate = std::move(wwwAuthenticate),
                        responseHeaders = std::move(responseHeaders),
                        msgBody = std::move(msgBody)]() mutable
                    {
                        onAuthenticationDone(
                            authenticationResult,
                            std::move(authInfo),
                            std::move(requestMessage),
                            std::move(wwwAuthenticate),
                            std::move(responseHeaders),
                            std::move(msgBody));
                    });
            });
    }

    void HttpServerConnection::onAuthenticationDone(
        bool authenticationResult,
        stree::ResourceContainer authInfo,
        nx_http::Message requestMessage,
        boost::optional<header::WWWAuthenticate> wwwAuthenticate,
        nx_http::HttpHeaders responseHeaders,
        std::unique_ptr<AbstractMsgBodySource> msgBody)
    {
        if (!authenticationResult)
        {
            sendUnauthorizedResponse(
                requestMessage.request->requestLine.version,
                std::move(wwwAuthenticate),
                std::move(responseHeaders),
                std::move(msgBody));
            return;
        }

        auto strongRef = shared_from_this();
        std::weak_ptr<HttpServerConnection> weakThis = strongRef;

        auto protoVersion = requestMessage.request->requestLine.version;
        auto sendResponseFunc = [this, weakThis, protoVersion](
            nx_http::Message&& response,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody) mutable
        {
            auto strongThis = weakThis.lock();
            if (!strongThis)
                return; //connection has been removed while request has been processed
            strongThis->post(
                [this, strongThis = std::move(strongThis),
                protoVersion = std::move(protoVersion),
                response = std::move(response),
                responseMsgBody = std::move(responseMsgBody)]() mutable
            {
                prepareAndSendResponse(
                    std::move(protoVersion),
                    std::move(response),
                    std::move(responseMsgBody));
            });
        };

        if (!m_httpMessageDispatcher ||
            !m_httpMessageDispatcher->dispatchRequest(
                this,
                std::move(requestMessage),
                std::move(authInfo),
                std::move(sendResponseFunc)))
        {
            //creating and sending error response
            nx_http::Message response(nx_http::MessageType::response);
            response.response->statusLine.statusCode = nx_http::StatusCode::notFound;
            return prepareAndSendResponse(
                std::move(protoVersion),
                std::move(response),
                nullptr);
        }
    }

    void HttpServerConnection::sendUnauthorizedResponse(
        const nx_http::MimeProtoVersion& protoVersion,
        boost::optional<header::WWWAuthenticate> wwwAuthenticate,
        nx_http::HttpHeaders responseHeaders,
        std::unique_ptr<AbstractMsgBodySource> msgBody)
    {
        nx_http::Message response(nx_http::MessageType::response);
        std::move(
            responseHeaders.begin(),
            responseHeaders.end(),
            std::inserter(response.response->headers, response.response->headers.end()));
        response.response->statusLine.statusCode = nx_http::StatusCode::unauthorized;
        if (wwwAuthenticate)
        {
            response.response->headers.emplace(
                header::WWWAuthenticate::NAME,
                wwwAuthenticate.get().serialized());
        }
        prepareAndSendResponse(
            protoVersion,
            std::move(response),
            std::move(msgBody));
    }

    void HttpServerConnection::prepareAndSendResponse(
        nx_http::MimeProtoVersion version,
        nx_http::Message&& msg,
        std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody )
    {
        msg.response->statusLine.version = std::move( version );
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

        if( responseMsgBody )
        {
            nx_http::insertOrReplaceHeader(
                &msg.response->headers,
                nx_http::HttpHeader( "Content-Type", responseMsgBody->mimeType() ) );

            const auto contentLength = responseMsgBody->contentLength();
            if( contentLength )
                nx_http::insertOrReplaceHeader(
                    &msg.response->headers,
                    nx_http::HttpHeader(
                        "Content-Length",
                        nx_http::StringType::number(
                            static_cast<qulonglong>( contentLength.get() ) ) ) );
        }
        else
        {
            nx_http::insertOrReplaceHeader(
                &msg.response->headers,
                nx_http::HttpHeader( "Content-Length", "0" ) );
        }

        if (responseMsgBody)
            responseMsgBody->bindToAioThread(getAioThread());

        //posting request to the queue 
        m_responseQueue.emplace_back(
            std::move(msg),
            std::move(responseMsgBody));
        if (m_responseQueue.size() == 1)
            sendNextResponse();
    }

    void HttpServerConnection::sendNextResponse()
    {
        NX_ASSERT(!m_responseQueue.empty());

        m_currentMsgBody = std::move(m_responseQueue.front().responseMsgBody);
        sendMessage(
            std::move(m_responseQueue.front().msg),
            std::bind(&HttpServerConnection::responseSent, this));
    }

    void HttpServerConnection::responseSent()
    {
        //TODO #ak check sendData error code
        if( !m_currentMsgBody )
        {
            fullMessageHasBeenSent();
            return;
        }

        readMoreMessageBodyData();
    }

    void HttpServerConnection::someMsgBodyRead(
        SystemError::ErrorCode errorCode,
        BufferType buf )
    {
        if( errorCode != SystemError::noError )
        {
            closeConnection(errorCode);
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

        sendData(
            std::move( buf ),
            std::bind( &HttpServerConnection::readMoreMessageBodyData, this ) );
    }

    void HttpServerConnection::readMoreMessageBodyData()
    {
        using namespace std::placeholders;
        m_currentMsgBody->readAsync(
            std::bind(&HttpServerConnection::someMsgBodyRead, this, _1, _2));
    }

    void HttpServerConnection::fullMessageHasBeenSent()
    {
        NX_ASSERT(!m_responseQueue.empty());
        m_responseQueue.pop_front();

        //if connection is NOT persistent then closing it
        m_currentMsgBody.reset();
        if (!m_isPersistent)
        {
            closeConnection(SystemError::noError);
            return;
        }

        if (!m_responseQueue.empty())
            sendNextResponse();
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
