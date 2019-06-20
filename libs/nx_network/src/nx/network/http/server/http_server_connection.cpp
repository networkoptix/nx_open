#include "http_server_connection.h"

#include <atomic>
#include <memory>

#include <QtCore/QDateTime>

#include <nx/network/socket_global.h>
#include <nx/utils/datetime.h>

#include "http_message_dispatcher.h"
#include "http_stream_socket_server.h"

namespace nx {
namespace network {
namespace http {

HttpServerConnection::HttpServerConnection(
    std::unique_ptr<AbstractStreamSocket> sock,
    nx::network::http::server::AbstractAuthenticationManager* const authenticationManager,
    nx::network::http::AbstractMessageDispatcher* const httpMessageDispatcher)
    :
    base_type(std::move(sock)),
    m_authenticationManager(authenticationManager),
    m_httpMessageDispatcher(httpMessageDispatcher)
{
    ++SocketGlobals::instance().debugCounters().httpServerConnectionCount;
}

HttpServerConnection::~HttpServerConnection()
{
    --SocketGlobals::instance().debugCounters().httpServerConnectionCount;
}

SocketAddress HttpServerConnection::clientEndpoint() const
{
    return m_clientEndpoint
        ? *m_clientEndpoint
        : socket()->getForeignAddress();
}

void HttpServerConnection::setPersistentConnectionEnabled(bool value)
{
    m_persistentConnectionEnabled = value;
}

void HttpServerConnection::processMessage(
    nx::network::http::Message requestMessage)
{
    if (requestMessage.type != nx::network::http::MessageType::request)
    {
        NX_DEBUG(this, lm("Unexpectedly received %1 from %2. Closing connection")
            .args(http::MessageType::toString(requestMessage.type), getForeignAddress()));
        closeConnection(SystemError::invalidData);
        return;
    }

    auto requestContext = prepareRequestProcessingContext(
        std::move(*requestMessage.request));

    NX_VERBOSE(this, lm("Processing request %1 received from %2")
        .args(requestContext->request.requestLine.url.toString(), getForeignAddress()));

    extractClientEndpoint(requestContext->request.headers);

    // TODO: #ak Incoming message body. Use AbstractMsgBodySource.

    checkForConnectionPersistency(requestContext->request);

    if (!m_authenticationManager)
    {
        onAuthenticationDone(
            nx::network::http::server::SuccessfulAuthenticationResult(),
            std::move(requestContext));
        return;
    }

    authenticate(std::move(requestContext));
}

void HttpServerConnection::extractClientEndpoint(const HttpHeaders& headers)
{
    extractClientEndpointFromXForwardedHeader(headers);
    if (!m_clientEndpoint)
        extractClientEndpointFromForwardedHeader(headers);

    if (m_clientEndpoint && m_clientEndpoint->port == 0)
        m_clientEndpoint->port = socket()->getForeignAddress().port;
}

void HttpServerConnection::extractClientEndpointFromXForwardedHeader(
    const HttpHeaders& headers)
{
    header::XForwardedFor xForwardedFor;
    auto xForwardedForIter = headers.find(header::XForwardedFor::NAME);
    if (xForwardedForIter != headers.end() &&
        xForwardedFor.parse(xForwardedForIter->second))
    {
        m_clientEndpoint.emplace(
            SocketAddress(xForwardedFor.client.constData(), 0));

        auto xForwardedPortIter = headers.find("X-Forwarded-Port");
        if (xForwardedPortIter != headers.end())
            m_clientEndpoint->port = xForwardedPortIter->second.toInt();
    }
}

void HttpServerConnection::extractClientEndpointFromForwardedHeader(
    const HttpHeaders& headers)
{
    header::Forwarded forwarded;
    auto forwardedIter = headers.find(header::Forwarded::NAME);
    if (forwardedIter != headers.end() &&
        forwarded.parse(forwardedIter->second) &&
        !forwarded.elements.front().for_.isEmpty())
    {
        m_clientEndpoint.emplace(forwarded.elements.front().for_);
    }
}

void HttpServerConnection::authenticate(
    std::unique_ptr<RequestContext> requestContext)
{
    const nx::network::http::Request& request = requestContext->request;
    auto strongRef = shared_from_this();
    std::weak_ptr<HttpServerConnection> weakThis = strongRef;
    m_authenticationManager->authenticate(
        *this,
        request,
        [this, weakThis = std::move(weakThis), requestContext = std::move(requestContext)](
            nx::network::http::server::AuthenticationResult authenticationResult) mutable
        {
            auto strongThis = weakThis.lock();
            if (!strongThis)
                return;

            strongThis->post(
                [this,
                    authenticationResult = std::move(authenticationResult),
                    requestContext = std::move(requestContext)]() mutable
                {
                    onAuthenticationDone(
                        std::move(authenticationResult),
                        std::move(requestContext));
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
    std::unique_ptr<RequestContext> requestContext)
{
    if (!socket())
    {
        // Connection has been removed while request authentication was in progress.
        closeConnection(SystemError::noError);
        return;
    }

    if (!authenticationResult.isSucceeded)
    {
        sendUnauthorizedResponse(
            std::move(requestContext),
            std::move(authenticationResult));
        return;
    }

    dispatchRequest(
        std::move(requestContext),
        std::move(authenticationResult));
}

std::unique_ptr<HttpServerConnection::RequestContext>
    HttpServerConnection::prepareRequestProcessingContext(
        nx::network::http::Request request)
{
    auto requestContext = std::make_unique<RequestContext>();
    requestContext->descriptor.sequence = ++m_lastRequestSequence;
    requestContext->descriptor.requestLine = request.requestLine;
    requestContext->descriptor.protocolToUpgradeTo =
        nx::network::http::getHeaderValue(request.headers, "Upgrade");
    requestContext->request = std::move(request);

    m_requestsBeingProcessed[requestContext->descriptor.sequence] = nullptr;

    return requestContext;
}

void HttpServerConnection::sendUnauthorizedResponse(
    std::unique_ptr<RequestContext> requestContext,
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
            authenticationResult.wwwAuthenticate->serialized());
    }

    prepareAndSendResponse(
        std::move(requestContext->descriptor),
        std::make_unique<ResponseMessageContext>(
            std::move(response),
            std::move(authenticationResult.msgBody),
            ConnectionEvents()));
}

void HttpServerConnection::dispatchRequest(
    std::unique_ptr<RequestContext> requestContext,
    nx::network::http::server::AuthenticationResult authenticationResult)
{
    auto strongRef = shared_from_this();
    std::weak_ptr<HttpServerConnection> weakThis = strongRef;

    auto sendResponseFunc =
        [this, weakThis, requestDescriptor = requestContext->descriptor](
            nx::network::http::Message response,
            std::unique_ptr<nx::network::http::AbstractMsgBodySource> responseMsgBody,
            ConnectionEvents connectionEvents) mutable
        {
            auto strongThis = weakThis.lock();
            if (!strongThis)
                return;

            processResponse(
                std::move(strongThis),
                std::move(requestDescriptor),
                std::make_unique<ResponseMessageContext>(
                    std::move(response),
                    std::move(responseMsgBody),
                    std::move(connectionEvents)));
        };

    if (!m_httpMessageDispatcher ||
        !m_httpMessageDispatcher->dispatchRequest(
            this,
            std::move(requestContext->request),
            std::move(authenticationResult.authInfo),
            std::move(sendResponseFunc)))
    {
        nx::network::http::Message response(nx::network::http::MessageType::response);
        response.response->statusLine.statusCode = nx::network::http::StatusCode::notFound;
        return prepareAndSendResponse(
            std::move(requestContext->descriptor),
            std::make_unique<ResponseMessageContext>(
                std::move(response),
                nullptr,
                ConnectionEvents()));
    }
}

void HttpServerConnection::processResponse(
    std::shared_ptr<HttpServerConnection> strongThis,
    RequestDescriptor requestDescriptor,
    std::unique_ptr<ResponseMessageContext> responseMessageContext)
{
    if (!socket())  //< Connection has been removed while request was being processed.
    {
        closeConnection(SystemError::noError);
        return;
    }

    NX_ASSERT(
        !responseMessageContext->msgBody ||
        nx::network::http::StatusCode::isMessageBodyAllowed(
            responseMessageContext->msg.response->statusLine.statusCode));

    strongThis->post(
        [this, strongThis = std::move(strongThis),
            requestDescriptor = std::move(requestDescriptor),
            responseMessageContext = std::move(responseMessageContext)]() mutable
        {
            if (!socket())
            {
                // Connection is closed.
                closeConnection(SystemError::noError);
                return;
            }

            prepareAndSendResponse(
                std::move(requestDescriptor),
                std::move(responseMessageContext));
        });
}

void HttpServerConnection::prepareAndSendResponse(
    RequestDescriptor requestDescriptor,
    std::unique_ptr<ResponseMessageContext> responseMessageContext)
{
    responseMessageContext->msg.response->statusLine.version =
        std::move(requestDescriptor.requestLine.version);

    responseMessageContext->msg.response->statusLine.reasonPhrase =
        nx::network::http::StatusCode::toString(
            responseMessageContext->msg.response->statusLine.statusCode);

    if (responseMessageContext->msgBody)
    {
        responseMessageContext->msgBody->bindToAioThread(getAioThread());
        if (responseMessageContext->msgBody->mimeType().isEmpty())
        {
            // Malformed message body?
            // TODO: #ak Add assert here and ensure no one uses this path.
            responseMessageContext->msgBody.reset();
        }
    }

    std::string responseContentLengthStr = "-";
    if (responseMessageContext->msgBody && responseMessageContext->msgBody->contentLength())
        responseContentLengthStr = std::to_string(*responseMessageContext->msgBody->contentLength());

    NX_VERBOSE(this, lm("%1 - %2 [%3] \"%4\" %5 %6")
        .args(getForeignAddress().address, "?" /*username*/, // TODO: #ak Fetch username.
            nx::utils::timestampToRfc2822(nx::utils::utcTime()),
            requestDescriptor.requestLine.toString(),
            responseMessageContext->msg.response->statusLine.statusCode,
            responseContentLengthStr));

    addResponseHeaders(
        requestDescriptor,
        responseMessageContext->msg.response,
        responseMessageContext->msgBody.get());

    scheduleResponseDelivery(
        requestDescriptor,
        std::move(responseMessageContext));
}

void HttpServerConnection::scheduleResponseDelivery(
    const RequestDescriptor& requestDescriptor,
    std::unique_ptr<ResponseMessageContext> responseMessageContext)
{
    m_requestsBeingProcessed[requestDescriptor.sequence] =
        std::move(responseMessageContext);

    while (!m_requestsBeingProcessed.empty()
        && m_requestsBeingProcessed.begin()->second != nullptr)
    {
        m_responseQueue.push_back(
            std::move(m_requestsBeingProcessed.begin()->second));
        m_requestsBeingProcessed.erase(m_requestsBeingProcessed.begin());

        if (m_responseQueue.size() == 1)
            sendNextResponse();
    }
}

void HttpServerConnection::addResponseHeaders(
    const RequestDescriptor& requestDescriptor,
    nx::network::http::Response* response,
    nx::network::http::AbstractMsgBodySource* responseMsgBody)
{
    static constexpr auto kYear = std::chrono::hours(24) * 365;

    nx::network::http::insertOrReplaceHeader(
        &response->headers,
        HttpHeader(header::Server::NAME, http::serverString()));

    nx::network::http::insertOrReplaceHeader(
        &response->headers,
        HttpHeader("Date", formatDateTime(QDateTime::currentDateTime())));

    const auto sslSocket = dynamic_cast<AbstractEncryptedStreamSocket*>(socket().get());
    if (sslSocket && sslSocket->isEncryptionEnabled())
    {
        header::StrictTransportSecurity strictTransportSecurity;
        strictTransportSecurity.maxAge = kYear;
        insertOrReplaceHeader(&response->headers, strictTransportSecurity);
    }

    addMessageBodyHeaders(response, responseMsgBody);

    if (response->statusLine.statusCode == nx::network::http::StatusCode::switchingProtocols)
    {
        if (response->headers.find("Upgrade") == response->headers.end())
            response->headers.emplace("Upgrade", requestDescriptor.protocolToUpgradeTo);

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
    else if (StatusCode::isMessageBodyAllowed(response->statusLine.statusCode))
    {
        nx::network::http::insertOrReplaceHeader(
            &response->headers,
            nx::network::http::HttpHeader("Content-Length", "0"));
    }
}

void HttpServerConnection::sendNextResponse()
{
    NX_ASSERT(!m_responseQueue.empty());

    m_currentMsgBody = std::move(m_responseQueue.front()->msgBody);
    sendMessage(
        std::move(m_responseQueue.front()->msg),
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

    NX_VERBOSE(this, lm("Not full message has been sent yet. Fetching more message body to send..."));
    readMoreMessageBodyData();
}

void HttpServerConnection::someMsgBodyRead(
    SystemError::ErrorCode errorCode,
    BufferType buf)
{
    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, lm("Error fetching message body to send. %1")
            .args(SystemError::toString(errorCode)));
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
    NX_VERBOSE(this, lm("Complete response message has been sent"));

    NX_ASSERT(!m_responseQueue.empty());
    if (m_responseQueue.front()->connectionEvents.onResponseHasBeenSent)
    {
        nx::utils::swapAndCall(
            m_responseQueue.front()->connectionEvents.onResponseHasBeenSent,
            this);
    }
    m_responseQueue.pop_front();

    // If connection is NOT persistent then closing it.
    m_currentMsgBody.reset();

    if (!socket() ||        //< Socket could be taken by event handler.
        !m_isPersistent)
    {
        closeConnection(SystemError::noError);
        return;
    }

    if (!m_responseQueue.empty())
        sendNextResponse();
}

void HttpServerConnection::checkForConnectionPersistency(
    const Request& request)
{
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
