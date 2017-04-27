#include "http_server_connection.h"

#include <memory>

#include <QtCore/QDateTime>

#include "http_message_dispatcher.h"
#include "http_stream_socket_server.h"

namespace nx_http {

HttpServerConnection::HttpServerConnection(
    StreamConnectionHolder<HttpServerConnection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> sock,
    nx_http::server::AbstractAuthenticationManager* const authenticationManager,
    nx_http::AbstractMessageDispatcher* const httpMessageDispatcher)
    :
    base_type(socketServer, std::move(sock)),
    m_authenticationManager(authenticationManager),
    m_httpMessageDispatcher(httpMessageDispatcher),
    m_isPersistent(false),
    m_persistentConnectionEnabled(true)
{
}

void HttpServerConnection::processMessage(nx_http::Message&& requestMessage)
{
    // TODO: #ak Incoming message body. Use AbstractMsgBodySource.

    // TODO: #ak pipelining support.
    // Makes sense to add sequence to sendResponseFunc and use it to queue responses.

    // Checking if connection is persistent.
    checkForConnectionPersistency(requestMessage);

    if (!m_authenticationManager)
    {
        onAuthenticationDone(
            nx_http::server::SuccessfulAuthenticationResult(),
            std::move(requestMessage));
        return;
    }

    const nx_http::Request& request = *requestMessage.request;
    auto strongRef = shared_from_this();
    std::weak_ptr<HttpServerConnection> weakThis = strongRef;
    m_authenticationManager->authenticate(
        *this,
        request,
        [this, weakThis = std::move(weakThis), requestMessage = std::move(requestMessage)](
            nx_http::server::AuthenticationResult authenticationResult) mutable
        {
            auto strongThis = weakThis.lock();
            if (!strongThis)
                return;

            if (!socket())  //< Connection has been removed while request authentication was in progress.
            {
                closeConnection(SystemError::noError);
                return;
            }

            strongThis->post(
                [this,
                    authenticationResult = std::move(authenticationResult),
                    requestMessage = std::move(requestMessage)]() mutable
                {
                    onAuthenticationDone(
                        std::move(authenticationResult),
                        std::move(requestMessage));
                });
        });
}

void HttpServerConnection::setPersistentConnectionEnabled(bool value)
{
    m_persistentConnectionEnabled = value;
}

void HttpServerConnection::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_currentMsgBody.reset();
}

void HttpServerConnection::onAuthenticationDone(
    nx_http::server::AuthenticationResult authenticationResult,
    nx_http::Message requestMessage)
{
    if (!authenticationResult.isSucceeded)
    {
        sendUnauthorizedResponse(
            requestMessage.request->requestLine.version,
            std::move(authenticationResult.wwwAuthenticate),
            std::move(authenticationResult.responseHeaders),
            std::move(authenticationResult.msgBody));
        return;
    }

    auto strongRef = shared_from_this();
    std::weak_ptr<HttpServerConnection> weakThis = strongRef;

    auto protoVersion = requestMessage.request->requestLine.version;
    auto sendResponseFunc =
        [this, weakThis, protoVersion](
            nx_http::Message response,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody,
            ConnectionEvents connectionEvents) mutable
        {
            auto strongThis = weakThis.lock();
            if (!strongThis)
                return;
            if (!socket())  //< Connection has been removed while request was being processed.
            {
                closeConnection(SystemError::noError);
                return;
            }

            strongThis->post(
                [this, strongThis = std::move(strongThis),
                    protoVersion = std::move(protoVersion),
                    response = std::move(response),
                    responseMsgBody = std::move(responseMsgBody),
                    connectionEvents = std::move(connectionEvents)]() mutable
                {
                    prepareAndSendResponse(
                        std::move(protoVersion),
                        std::move(response),
                        std::move(responseMsgBody),
                        std::move(connectionEvents));
                });
        };

    if (!m_httpMessageDispatcher ||
        !m_httpMessageDispatcher->dispatchRequest(
            this,
            std::move(requestMessage),
            std::move(authenticationResult.authInfo),
            std::move(sendResponseFunc)))
    {
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
    std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody,
    ConnectionEvents connectionEvents)
{
    msg.response->statusLine.version = std::move( version );
    msg.response->statusLine.reasonPhrase =
        nx_http::StatusCode::toString( msg.response->statusLine.statusCode );

    nx_http::insertOrReplaceHeader(
        &msg.response->headers,
        nx_http::HttpHeader(nx_http::header::Server::NAME, nx_http::serverString() ) );
    nx_http::insertOrReplaceHeader(
        &msg.response->headers,
        nx_http::HttpHeader( "Date", nx_http::formatDateTime( QDateTime::currentDateTime() ) ) );

    if (responseMsgBody)
    {
        const auto contentType = responseMsgBody->mimeType();
        if (contentType.isEmpty())
        {
            // Switching protocols, no content information should be provided at all
            responseMsgBody.reset();
        }
        else
        {
            nx_http::insertOrReplaceHeader(
                &msg.response->headers,
                nx_http::HttpHeader( "Content-Type", contentType ) );

            const auto contentLength = responseMsgBody->contentLength();
            if( contentLength )
                nx_http::insertOrReplaceHeader(
                    &msg.response->headers,
                    nx_http::HttpHeader(
                        "Content-Length",
                        nx_http::StringType::number(
                            static_cast<qulonglong>( contentLength.get() ) ) ) );
        }
    }
    else
    {
        nx_http::insertOrReplaceHeader(
            &msg.response->headers,
            nx_http::HttpHeader( "Content-Length", "0" ) );
    }

    if (responseMsgBody)
        responseMsgBody->bindToAioThread(getAioThread());

    m_responseQueue.emplace_back(
        std::move(msg),
        std::move(responseMsgBody),
        std::move(connectionEvents));
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
    // TODO: #ak check sendData error code.
    if (!m_currentMsgBody)
    {
        fullMessageHasBeenSent();
        return;
    }

    readMoreMessageBodyData();
}

void HttpServerConnection::someMsgBodyRead(
    SystemError::ErrorCode errorCode,
    BufferType buf)
{
    if (errorCode != SystemError::noError)
    {
        closeConnection(errorCode);
        return;
    }

    if (buf.isEmpty())
    {
        // Done with message body.
        fullMessageHasBeenSent();
        return;
    }

    // TODO #ak read and send message body async.
    //    Move async reading/writing to some separate class (async pipe) to enable reusage.

    sendData(
        std::move(buf),
        std::bind(&HttpServerConnection::readMoreMessageBodyData, this));
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
    if (m_responseQueue.front().connectionEvents.onResponseHasBeenSent)
    {
        auto handler =
            std::move(m_responseQueue.front().connectionEvents.onResponseHasBeenSent);
        handler(this);
    }
    m_responseQueue.pop_front();

    //if connection is NOT persistent then closing it
    m_currentMsgBody.reset();

    if (!socket() ||           //< socket could be taken by event handler
        !m_isPersistent)
    {
        closeConnection(SystemError::noError);
        return;
    }

    if (!m_responseQueue.empty())
        sendNextResponse();
}

void HttpServerConnection::checkForConnectionPersistency(const Message& msg)
{
    if (msg.type != MessageType::request)
        return;

    const auto& request = *msg.request;

    m_isPersistent = false;
    if (m_persistentConnectionEnabled)
    {
        if (request.requestLine.version == nx_http::http_1_1)
            m_isPersistent = nx_http::getHeaderValue(request.headers, "Connection").toLower() != "close";
        else if (request.requestLine.version == nx_http::http_1_0)
            m_isPersistent = nx_http::getHeaderValue(request.headers, "Connection").toLower() == "keep-alive";
    }
}

} // namespace nx_http
