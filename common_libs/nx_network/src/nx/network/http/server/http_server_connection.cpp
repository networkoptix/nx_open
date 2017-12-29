#include "http_server_connection.h"

#include <memory>

#include <QtCore/QDateTime>

#include "http_message_dispatcher.h"
#include "http_stream_socket_server.h"

namespace nx {
namespace network {
namespace http {

HttpServerConnection::HttpServerConnection(
    nx::network::server::StreamConnectionHolder<HttpServerConnection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> sock,
    nx::network::http::server::AbstractAuthenticationManager* const authenticationManager,
    nx::network::http::AbstractMessageDispatcher* const httpMessageDispatcher)
    :
    base_type(socketServer, std::move(sock)),
    m_authenticationManager(authenticationManager),
    m_httpMessageDispatcher(httpMessageDispatcher),
    m_isPersistent(false),
    m_persistentConnectionEnabled(true)
{
}

void HttpServerConnection::setPersistentConnectionEnabled(bool value)
{
    m_persistentConnectionEnabled = value;
}

void HttpServerConnection::processMessage(nx::network::http::Message requestMessage)
{
    // TODO: #ak Incoming message body. Use AbstractMsgBodySource.

    // TODO: #ak pipelining support.
    // Makes sense to add sequence to sendResponseFunc and use it to queue responses.

    // Checking if connection is persistent.
    checkForConnectionPersistency(requestMessage);

    if (!m_authenticationManager)
    {
        onAuthenticationDone(
            nx::network::http::server::SuccessfulAuthenticationResult(),
            std::move(requestMessage));
        return;
    }

    authenticate(std::move(requestMessage));
}

void HttpServerConnection::authenticate(nx::network::http::Message requestMessage)
{
    const nx::network::http::Request& request = *requestMessage.request;
    auto strongRef = shared_from_this();
    std::weak_ptr<HttpServerConnection> weakThis = strongRef;
    m_authenticationManager->authenticate(
        *this,
        request,
        [this, weakThis = std::move(weakThis), requestMessage = std::move(requestMessage)](
            nx::network::http::server::AuthenticationResult authenticationResult) mutable
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

void HttpServerConnection::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_currentMsgBody.reset();
}

void HttpServerConnection::onAuthenticationDone(
    nx::network::http::server::AuthenticationResult authenticationResult,
    nx::network::http::Message requestMessage)
{
    RequestProcessingContext processingContext =
        prepareRequestProcessingContext(*requestMessage.request);

    if (!authenticationResult.isSucceeded)
    {
        sendUnauthorizedResponse(
            std::move(processingContext),
            std::move(authenticationResult));
        return;
    }

    dispatchRequest(
        std::move(processingContext),
        std::move(authenticationResult),
        std::move(requestMessage));
}

HttpServerConnection::RequestProcessingContext
    HttpServerConnection::prepareRequestProcessingContext(
        const nx::network::http::Request& request)
{
    RequestProcessingContext processingContext;
    processingContext.httpVersion = request.requestLine.version;
    processingContext.protocolToUpgradeTo =
        nx::network::http::getHeaderValue(request.headers, "Upgrade");
    return processingContext;
}

void HttpServerConnection::sendUnauthorizedResponse(
    RequestProcessingContext processingContext,
    nx::network::http::server::AuthenticationResult authenticationResult)
{
    nx::network::http::Message response(nx::network::http::MessageType::response);
    std::move(
        authenticationResult.responseHeaders.begin(),
        authenticationResult.responseHeaders.end(),
        std::inserter(response.response->headers, response.response->headers.end()));
    response.response->statusLine.statusCode = nx::network::http::StatusCode::unauthorized;
    if (authenticationResult.wwwAuthenticate)
    {
        response.response->headers.emplace(
            header::WWWAuthenticate::NAME,
            authenticationResult.wwwAuthenticate.get().serialized());
    }

    prepareAndSendResponse(
        processingContext,
        ResponseMessageContext(
            std::move(response),
            std::move(authenticationResult.msgBody),
            ConnectionEvents()));
}

void HttpServerConnection::dispatchRequest(
    RequestProcessingContext processingContext,
    nx::network::http::server::AuthenticationResult authenticationResult,
    nx::network::http::Message requestMessage)
{
    auto strongRef = shared_from_this();
    std::weak_ptr<HttpServerConnection> weakThis = strongRef;

    auto sendResponseFunc =
        [this, weakThis, processingContext](
            nx::network::http::Message response,
            std::unique_ptr<nx::network::http::AbstractMsgBodySource> responseMsgBody,
            ConnectionEvents connectionEvents) mutable
        {
            auto strongThis = weakThis.lock();
            if (!strongThis)
                return;

            processResponse(
                std::move(strongThis),
                std::move(processingContext),
                ResponseMessageContext(
                    std::move(response),
                    std::move(responseMsgBody),
                    std::move(connectionEvents)));
        };

    if (!m_httpMessageDispatcher ||
        !m_httpMessageDispatcher->dispatchRequest(
            this,
            std::move(requestMessage),
            std::move(authenticationResult.authInfo),
            std::move(sendResponseFunc)))
    {
        nx::network::http::Message response(nx::network::http::MessageType::response);
        response.response->statusLine.statusCode = nx::network::http::StatusCode::notFound;
        return prepareAndSendResponse(
            std::move(processingContext),
            ResponseMessageContext(std::move(response), nullptr, ConnectionEvents()));
    }
}

void HttpServerConnection::processResponse(
    std::shared_ptr<HttpServerConnection> strongThis,
    RequestProcessingContext processingContext,
    ResponseMessageContext responseMessageContext)
{
    if (!socket())  //< Connection has been removed while request was being processed.
    {
        closeConnection(SystemError::noError);
        return;
    }

    NX_ASSERT(
        !responseMessageContext.msgBody ||
        nx::network::http::StatusCode::isMessageBodyAllowed(
            responseMessageContext.msg.response->statusLine.statusCode));

    strongThis->post(
        [this, strongThis = std::move(strongThis),
            processingContext = std::move(processingContext),
            responseMessageContext = std::move(responseMessageContext)]() mutable
        {
            prepareAndSendResponse(
                std::move(processingContext),
                std::move(responseMessageContext));
        });
}

void HttpServerConnection::prepareAndSendResponse(
    RequestProcessingContext processingContext,
    ResponseMessageContext responseMessageContext)
{
    responseMessageContext.msg.response->statusLine.version =
        std::move(processingContext.httpVersion);
    responseMessageContext.msg.response->statusLine.reasonPhrase =
        nx::network::http::StatusCode::toString(
            responseMessageContext.msg.response->statusLine.statusCode);

    if (responseMessageContext.msgBody)
    {
        responseMessageContext.msgBody->bindToAioThread(getAioThread());
        if (responseMessageContext.msgBody->mimeType().isEmpty())
        {
            // Malformed message body?
            // TODO: #ak Add assert here and ensure no one uses this path.
            responseMessageContext.msgBody.reset();
        }
    }

    addResponseHeaders(
        processingContext,
        responseMessageContext.msg.response,
        responseMessageContext.msgBody.get());

    m_responseQueue.push_back(std::move(responseMessageContext));
    if (m_responseQueue.size() == 1)
        sendNextResponse();
}

void HttpServerConnection::addResponseHeaders(
    const RequestProcessingContext& processingContext,
    nx::network::http::Response* response,
    nx::network::http::AbstractMsgBodySource* responseMsgBody)
{
    static const auto kYear = std::chrono::hours(24) * 365;

    nx::network::http::insertOrReplaceHeader(
        &response->headers,
        nx::network::http::HttpHeader(nx::network::http::header::Server::NAME, nx::network::http::serverString()));
    nx::network::http::insertOrReplaceHeader(
        &response->headers,
        nx::network::http::HttpHeader("Date", nx::network::http::formatDateTime(QDateTime::currentDateTime())));

    const auto sslSocket = dynamic_cast<AbstractEncryptedStreamSocket*>(socket().get());
    if (sslSocket && sslSocket->isEncryptionEnabled())
    {
        nx::network::http::header::StrictTransportSecurity strictTransportSecurity;
        strictTransportSecurity.maxAge = kYear;
        nx::network::http::insertOrReplaceHeader(&response->headers, strictTransportSecurity);
    }

    addMessageBodyHeaders(response, responseMsgBody);

    if (response->statusLine.statusCode == nx::network::http::StatusCode::switchingProtocols)
    {
        if (response->headers.find("Upgrade") == response->headers.end())
            response->headers.emplace("Upgrade", processingContext.protocolToUpgradeTo);
        nx::network::http::insertOrReplaceHeader(
            &response->headers,
            HttpHeader("Connection", "Upgrade"));
    }
}

void HttpServerConnection::addMessageBodyHeaders(
    nx::network::http::Response* response,
    nx::network::http::AbstractMsgBodySource* responseMsgBody)
{
    if (responseMsgBody)
    {
        nx::network::http::insertOrReplaceHeader(
            &response->headers,
            nx::network::http::HttpHeader("Content-Type", responseMsgBody->mimeType()));

        const auto contentLength = responseMsgBody->contentLength();
        if (contentLength)
        {
            nx::network::http::insertOrReplaceHeader(
                &response->headers,
                nx::network::http::HttpHeader(
                    "Content-Length",
                    nx::network::http::StringType::number(
                        static_cast<qulonglong>(contentLength.get()))));
        }
    }
    else if (nx::network::http::StatusCode::isMessageBodyAllowed(response->statusLine.statusCode))
    {
        nx::network::http::insertOrReplaceHeader(
            &response->headers,
            nx::network::http::HttpHeader("Content-Length", "0"));
    }
}

void HttpServerConnection::sendNextResponse()
{
    NX_ASSERT(!m_responseQueue.empty());

    m_currentMsgBody = std::move(m_responseQueue.front().msgBody);
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
        if (!m_currentMsgBody->contentLength())
        {
            // The only way to signal about the end of message body is to close a connection
            // if Content-Length is not specified.
            // Connection will be closed after sending this response if noone takes socket
            // for any reason (like establishing some tunnel, etc...).
            m_isPersistent = false;
        }

        // Done with message body.
        fullMessageHasBeenSent();
        return;
    }

    // TODO: #ak read and send message body async.
    //  Move async reading/writing to some separate class (async pipe) to enable reusage.
    //  AsyncChannelUnidirectionalBridge can serve that purpose.

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
    if (msg.type != nx::network::http::MessageType::request)
        return;

    const auto& request = *msg.request;

    m_isPersistent = false;
    if (m_persistentConnectionEnabled)
    {
        if (request.requestLine.version == nx::network::http::http_1_1)
            m_isPersistent = nx::network::http::getHeaderValue(request.headers, "Connection").toLower() != "close";
        else if (request.requestLine.version == nx::network::http::http_1_0)
            m_isPersistent = nx::network::http::getHeaderValue(request.headers, "Connection").toLower() == "keep-alive";
    }
}

} // namespace nx
} // namespace network
} // namespace http
