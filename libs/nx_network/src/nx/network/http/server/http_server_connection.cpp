// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_connection.h"

#include <atomic>
#include <memory>

#include <nx/network/aio/async_channel_bridge.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/datetime.h>

#include "http_message_dispatcher.h"
#include "http_stream_socket_server.h"

namespace nx::network::http {

static std::atomic<std::uint64_t> sConnectionId{0};

HttpServerConnection::HttpServerConnection(
    std::unique_ptr<AbstractStreamSocket> sock,
    nx::network::http::AbstractRequestHandler* requestHandler,
    std::optional<SocketAddress> addressToRedirect)
    :
    base_type(std::move(sock)),
    m_requestHandler(requestHandler),
    m_addressToRedirect(std::move(addressToRedirect)),
    m_attrs{
        .id = ++sConnectionId,
        .isSsl = isSsl(),
        .sourceAddr = socket()->getForeignAddress(),
        .localAddr = socket()->getLocalAddress()}
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().httpServerConnectionCount;

    m_closeHandlerSubscriptionId = registerCloseHandler(
        [this](SystemError::ErrorCode reason, auto /*connectionDestroyed*/)
        {
            cleanUpOnConnectionClosure(reason);
        });
}

HttpServerConnection::~HttpServerConnection()
{
    removeCloseHandler(m_closeHandlerSubscriptionId);

    --SocketGlobals::instance().debugCounters().httpServerConnectionCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

std::unique_ptr<AbstractStreamSocket> HttpServerConnection::takeSocket()
{
    auto connection = base_type::takeSocket();
    // Taking socket prohibits queued responses from being sent.

    const auto pendingRequests =
        nx::utils::join(m_responseQueue, "; ",
            [](const auto& val) { return val->requestLine.toString(); });
    NX_ASSERT(m_responseQueue.empty(), pendingRequests);
    return connection;
}

std::unique_ptr<nx::network::http::AbstractMsgBodySource>
    HttpServerConnection::takeCurrentMsgBody()
{
    return std::exchange(m_currentMsgBody, nullptr);
}

SocketAddress HttpServerConnection::lastRequestSource() const
{
    return m_clientEndpoint
        ? *m_clientEndpoint
        : socket()->getForeignAddress();
}

void HttpServerConnection::setPersistentConnectionEnabled(bool value)
{
    m_persistentConnectionEnabled = value;
}

void HttpServerConnection::setExtraResponseHeaders(HttpHeaders responseHeaders)
{
    m_extraResponseHeaders = std::move(responseHeaders);
}

void HttpServerConnection::setOnResponseSent(
    nx::utils::MoveOnlyFunc<void(std::chrono::microseconds)> handler)
{
    m_responseSentHandler = std::move(handler);
}

void HttpServerConnection::processMessage(
    nx::network::http::Message requestMessage)
{
    // NOTE: The message contains only headers. No message body yet.

    if (requestMessage.type != nx::network::http::MessageType::request)
    {
        NX_DEBUG(this, "Unexpectedly received %1 from %2. Closing connection",
            http::MessageType::toString(requestMessage.type), getForeignAddress());
        closeConnection(SystemError::invalidData);
        return;
    }

    extractClientEndpoint(requestMessage.request->headers);

    auto requestContext = prepareRequestAuthContext(std::move(*requestMessage.request));

    NX_VERBOSE(this, "Processing request %1 received from %2",
        requestContext->request.requestLine, getForeignAddress());

    checkForConnectionPersistency(requestContext->request);

    if (m_addressToRedirect)
    {
        nx::network::http::Message response(nx::network::http::MessageType::response);
        response.response->statusLine.statusCode = StatusCode::movedPermanently;
        const auto url = nx::network::url::Builder(requestContext->request.requestLine.url)
            .setScheme(kSecureUrlSchemeName).setEndpoint(*m_addressToRedirect).toUrl();
        response.response->headers.emplace("Location", url.toStdString());

        prepareAndSendResponse(
            std::move(requestContext->descriptor),
            std::make_unique<ResponseMessageContext>(
                requestContext->request.requestLine,
                std::move(response),
                /*response body*/ nullptr,
                ConnectionEvents(),
                requestContext->requestReceivedTime));
        return;
    }

    invokeRequestHandler(std::move(requestContext));
}

void HttpServerConnection::processSomeMessageBody(nx::Buffer buffer)
{
    if (m_currentRequestBodyWriter)
        m_currentRequestBodyWriter->writeBodyData(std::move(buffer));
}

void HttpServerConnection::processMessageEnd()
{
    if (m_currentRequestBodyWriter)
    {
        nx::utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
        m_currentRequestBodyWriter->writeEof(SystemError::noError);
        if (watcher.interrupted())
            return; //< NOTE: EOF can destroy connection.

        m_currentRequestBodyWriter = nullptr;
        if (m_markedForClosure)
        {
            NX_VERBOSE(this, "Closing marked connection from %1", getForeignAddress());
            closeConnection(*m_markedForClosure);
        }
    }
}

void HttpServerConnection::extractClientEndpoint(const HttpHeaders& headers)
{
    m_clientEndpoint = std::nullopt;

    extractClientEndpointFromXForwardedHeader(headers);
    if (!m_clientEndpoint)
        extractClientEndpointFromForwardedHeader(headers);

    if (m_clientEndpoint && m_clientEndpoint->port == 0)
        m_clientEndpoint->port = socket()->getForeignAddress().port;

    if (!m_clientEndpoint)
        m_clientEndpoint = socket()->getForeignAddress();
}

void HttpServerConnection::extractClientEndpointFromXForwardedHeader(
    const HttpHeaders& headers)
{
    header::XForwardedFor xForwardedFor;
    auto xForwardedForIter = headers.find(header::XForwardedFor::NAME);
    if (xForwardedForIter != headers.end() &&
        xForwardedFor.parse(xForwardedForIter->second))
    {
        m_clientEndpoint.emplace(SocketAddress(xForwardedFor.client, 0));

        auto xForwardedPortIter = headers.find("X-Forwarded-Port");
        if (xForwardedPortIter != headers.end())
            m_clientEndpoint->port = nx::utils::stoi(xForwardedPortIter->second);
    }
}

void HttpServerConnection::extractClientEndpointFromForwardedHeader(
    const HttpHeaders& headers)
{
    header::Forwarded forwarded;
    auto forwardedIter = headers.find(header::Forwarded::NAME);
    if (forwardedIter != headers.end() &&
        forwarded.parse(forwardedIter->second) &&
        !forwarded.elements.front().for_.empty())
    {
        m_clientEndpoint.emplace(forwarded.elements.front().for_);
    }
}

void HttpServerConnection::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_currentMsgBody.reset();
    m_bridge.reset();
}

std::unique_ptr<HttpServerConnection::RequestAuthContext>
    HttpServerConnection::prepareRequestAuthContext(
        nx::network::http::Request request)
{
    auto requestContext = std::make_unique<RequestAuthContext>();
    requestContext->requestReceivedTime = clock_type::now();
    requestContext->descriptor.sequence = ++m_lastRequestSequence;
    requestContext->descriptor.requestLine = request.requestLine;
    requestContext->descriptor.protocolToUpgradeTo =
        nx::network::http::getHeaderValue(request.headers, "Upgrade");
    requestContext->request = std::move(request);
    requestContext->clientEndpoint = lastRequestSource();

    m_requestsBeingProcessed[requestContext->descriptor.sequence] = nullptr;

    std::optional<uint64_t> contentLength;
    if (auto it = requestContext->request.headers.find("Content-Length");
        it != requestContext->request.headers.end())
    {
        contentLength = nx::utils::stoull(it->second);
    }

    auto body = std::make_unique<WritableMessageBody>(
        getHeaderValue(requestContext->request.headers, "Content-Type"),
        contentLength);
    m_currentRequestBodyWriter = body->writer();
    requestContext->body = std::move(body);

    return requestContext;
}

void HttpServerConnection::invokeRequestHandler(
    std::unique_ptr<RequestAuthContext> requestAuthContext)
{
    auto strongRef = shared_from_this();
    std::weak_ptr<HttpServerConnection> weakThis = strongRef;

    auto sendResponseFunc =
        [this, weakThis, requestDescriptor = std::exchange(requestAuthContext->descriptor, {}),
            requestReceivedTime = requestAuthContext->requestReceivedTime](
                RequestResult result) mutable
        {
            auto strongThis = weakThis.lock();
            if (!strongThis)
                return;

            Message response(http::MessageType::response);
            response.response->statusLine.statusCode = result.statusCode;
            response.response->statusLine.reasonPhrase = StatusCode::toString(result.statusCode);
            response.response->statusLine.version = requestDescriptor.requestLine.version;
            response.response->headers = std::move(result.headers);

            auto requestLine = requestDescriptor.requestLine;
            processResponse(
                std::move(strongThis),
                std::move(requestDescriptor),
                std::make_unique<ResponseMessageContext>(
                    std::move(requestLine),
                    std::move(response),
                    std::move(result.body),
                    std::move(result.connectionEvents),
                    requestReceivedTime));
        };

    if (!m_requestHandler)
        return sendResponseFunc(RequestResult(StatusCode::notFound));

    m_requestHandler->serve(
        buildRequestContext(std::move(*requestAuthContext)),
        std::move(sendResponseFunc));
}

RequestContext HttpServerConnection::buildRequestContext(
    RequestAuthContext requestAuthContext)
{
    return RequestContext(
        m_attrs,
        shared_from_this(),
        std::move(requestAuthContext.clientEndpoint),
        {},
        std::move(requestAuthContext.request),
        std::move(requestAuthContext.body));
}

void HttpServerConnection::processResponse(
    std::shared_ptr<HttpServerConnection> strongThis,
    RequestDescriptor requestDescriptor,
    std::unique_ptr<ResponseMessageContext> responseMessageContext)
{
    post(
        [this, strongThis = std::move(strongThis),
            requestDescriptor = std::move(requestDescriptor),
            responseMessageContext = std::move(responseMessageContext)]() mutable
        {
            if (!socket())
            {
                // Connection is closed.
                NX_VERBOSE(this, "Closing connection since socket is not available to send response (2). "
                    "Last request %1", requestDescriptor.requestLine);
                closeConnection(SystemError::noError);
                return;
            }

            NX_ASSERT(
                !responseMessageContext->msgBody ||
                nx::network::http::StatusCode::isMessageBodyAllowed(
                    responseMessageContext->msg.response->statusLine.statusCode));

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
        responseMessageContext->msgBody->bindToAioThread(getAioThread());

    std::string responseContentLengthStr = "-";
    if (responseMessageContext->msgBody && responseMessageContext->msgBody->contentLength())
        responseContentLengthStr = std::to_string(*responseMessageContext->msgBody->contentLength());

    const auto requestProcessedIn = std::chrono::floor<std::chrono::milliseconds>(
        clock_type::now() - responseMessageContext->requestReceivedTime);

    NX_VERBOSE(this, "%1 - %2 [%3] \"%4\" %5 %6 %7",
            getForeignAddress().address, "?" /*username*/, // TODO: #akolesnikov Fetch username.
            nx::utils::timestampToRfc2822(nx::utils::utcTime()),
            requestDescriptor.requestLine.toString(),
            responseMessageContext->msg.response->statusLine.statusCode,
            responseContentLengthStr,
            requestProcessedIn.count());

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

    for (const auto& hdr : m_extraResponseHeaders)
    {
        // Don't replace headers added by the request processors.
        if (!response->headers.contains(hdr.first))
            response->headers.insert(hdr);
    }

    // ... Except for the Server header. It should be customizable and only added if not present.
    if (!response->headers.contains(header::Server::NAME))
    {
        nx::network::http::insertOrReplaceHeader(
            &response->headers,
            HttpHeader(header::Server::NAME, http::serverString()));
    }

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
                    std::to_string(*contentLength)));
        }
    }
    else if (StatusCode::isMessageBodyAllowed(response->statusLine.statusCode) &&
        response->headers.find("Content-Length") == response->headers.end()) //< Not overriding existing header.
    {
        nx::network::http::insertOrReplaceHeader(
            &response->headers,
            {"Content-Length", "0"});
    }
}

void HttpServerConnection::sendNextResponse()
{
    NX_ASSERT(!m_responseQueue.empty());

    m_currentMsgBody = std::exchange(m_responseQueue.front()->msgBody, nullptr);

    m_chunkedBodyParser = std::nullopt;
    if (nx::utils::contains(
            getHeaderValue(m_responseQueue.front()->msg.response->headers, "Transfer-Encoding"),
            "chunked"))
    {
        m_chunkedBodyParser = ChunkedStreamParser();
    }

    sendMessage(
        std::exchange(m_responseQueue.front()->msg, {}),
        std::bind(
            &HttpServerConnection::responseSent, this,
            m_responseQueue.front()->requestReceivedTime));
}

void HttpServerConnection::responseSent(const time_point& requestReceivedTime)
{
    if (m_responseSentHandler)
    {
        using namespace std::chrono;
        m_responseSentHandler(
            duration_cast<microseconds>(clock_type::now() - requestReceivedTime));
    }

    // TODO: #akolesnikov check sendData error code.
    if (!m_currentMsgBody)
    {
        fullMessageHasBeenSent();
        return;
    }

    readMoreMessageBodyData();
}

void HttpServerConnection::someMsgBodyRead(
    SystemError::ErrorCode errorCode,
    nx::Buffer buf)
{
    NX_VERBOSE(this, "Got %1 bytes of message body. Error code %2",
        buf.size(), SystemError::toString(errorCode));

    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, "Error fetching message body to send. %1", SystemError::toString(errorCode));
        closeConnection(errorCode);
        return;
    }

    if (buf.empty())
    {
        if (!m_currentMsgBody->contentLength() && !m_chunkedBodyParser)
        {
            // The only way to signal about the end of message body is to close a connection
            // if Content-Length is not specified.
            // Connection will be closed after sending this response if no one takes socket
            // for any reason (like establishing some tunnel, etc...).
            m_isPersistent = false;

            NX_VERBOSE(this, "Switching connection from %1 to non-persistent mode to signal the end "
                "of the request %2 response body", m_clientEndpoint,
                !m_responseQueue.empty() ? m_responseQueue.front()->requestLine.url.toStdString() : "");
        }

        // Done with message body.
        fullMessageHasBeenSent();
        return;
    }

    if (m_chunkedBodyParser)
    {
        m_chunkedBodyParser->parse(buf, [](auto&&...) {});
        if (m_chunkedBodyParser->eof())
        {
            // Reached end of HTTP/1.1 chunked message body.
            sendData(
                std::move(buf),
                [this](auto&&...) { fullMessageHasBeenSent(); });
            return;
        }
    }

    // TODO: #akolesnikov read and send message body async.
    //  Move async reading/writing to some separate class (async pipe) to enable reusage.
    //  AsyncChannelUnidirectionalBridge can serve that purpose.

    sendData(
        std::move(buf),
        [this](SystemError::ErrorCode sendResult)
        {
            if (sendResult != SystemError::noError)
            {
                NX_VERBOSE(this, "Failed to send message body. %1",
                    SystemError::toString(sendResult));
                return;
            }

            readMoreMessageBodyData();
        });
}

void HttpServerConnection::readMoreMessageBodyData()
{
    NX_VERBOSE(this, "Not full message has been sent yet. Fetching more message body to send...");

    m_currentMsgBody->readAsync(
        [this](auto&&... args) { someMsgBodyRead(std::forward<decltype(args)>(args)...); });
}

void HttpServerConnection::fullMessageHasBeenSent()
{
    NX_VERBOSE(this, "Complete response message has been sent");

    NX_ASSERT(!m_responseQueue.empty());

    const auto onResponseHasBeenSent =
        std::exchange(m_responseQueue.front()->connectionEvents.onResponseHasBeenSent, nullptr);
    m_responseQueue.pop_front();

    if (onResponseHasBeenSent)
    {
        nx::utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
        onResponseHasBeenSent(this);
        if (watcher.interrupted())
            return; //< NOTE: The handler is allowed to close the connection.
    }

    // If connection is NOT persistent then closing it.
    m_currentMsgBody.reset();

    if (m_bridge)
        return;

    if (!socket())        //< Socket could be taken by event handler.
    {
        NX_ASSERT(m_responseQueue.empty());
        NX_VERBOSE(this, "Closing connection since socket is not available");
        return closeConnection(SystemError::noError);
    }

    if (!m_isPersistent || !m_persistentConnectionEnabled)
    {
        NX_VERBOSE(this, "Marking connection from %1 for closure",
            getForeignAddress());
        return closeConnectionAfterReceivingCompleteRequest(SystemError::noError);
    }

    if (!m_responseQueue.empty())
        sendNextResponse();
}

void HttpServerConnection::startConnectionBridging(
    std::unique_ptr<AbstractCommunicatingSocket> targetConnection)
{
    auto remoteAddr = getForeignAddress();

    NX_VERBOSE(this, "Bridging %1 and %2", remoteAddr, targetConnection);

    m_bridge = aio::makeAsyncChannelBridge(takeSocket(), std::move(targetConnection));
    m_bridge->bindToAioThread(getAioThread());
    m_bridge->start(
        [this, remoteAddr = std::move(remoteAddr)](SystemError::ErrorCode resultCode)
        {
            NX_VERBOSE(this, "Closing connection from %1 since bridging completed with %2",
                remoteAddr, SystemError::toString(resultCode));
            closeConnection(resultCode);
        });
}

std::size_t HttpServerConnection::pendingRequestCount() const
{
    return m_requestsBeingProcessed.size();
}

std::size_t HttpServerConnection::pendingResponseCount() const
{
    return m_responseQueue.size();
}

const ConnectionAttrs& HttpServerConnection::attrs() const
{
    return m_attrs;
}

void HttpServerConnection::checkForConnectionPersistency(
    const Request& request)
{
    const auto isPersistentBak = m_isPersistent;

    m_isPersistent = false;
    if (m_persistentConnectionEnabled)
    {
        if (request.requestLine.version == nx::network::http::http_1_1)
            m_isPersistent = nx::utils::stricmp(getHeaderValue(request.headers, "Connection"), "close") != 0;
        else if (request.requestLine.version == nx::network::http::http_1_0)
            m_isPersistent = nx::utils::stricmp(getHeaderValue(request.headers, "Connection"), "keep-alive") == 0;
    }

    if (m_isPersistent != isPersistentBak)
    {
        NX_VERBOSE(this, "Changed persistence of the connection from %1 to %2 based on request %3 (%4)",
            getForeignAddress(), m_isPersistent, request.requestLine,
            getHeaderValue(request.headers, "Connection"));
    }
}

void HttpServerConnection::closeConnectionAfterReceivingCompleteRequest(
    SystemError::ErrorCode reason)
{
    // [RFC2616, 8.2.2] allows the server to respond before receiving the complete request body.
    // But, the server may still continue reading the request body.
    // So, waiting until the server is done doing that.

    if (!m_currentRequestBodyWriter)
    {
        NX_VERBOSE(this, "Closing connection from %1 due to marking. %2",
            getForeignAddress(), SystemError::toString(reason));
        closeConnection(reason);
    }
    else
    {
        NX_VERBOSE(this, "Marking connection from %1 for closure. %2",
            getForeignAddress(), SystemError::toString(reason));
        m_markedForClosure = reason;
    }
}

void HttpServerConnection::onRequestBodyReaderCompleted()
{
    m_currentRequestBodyWriter = nullptr;
    if (m_markedForClosure)
    {
        NX_VERBOSE(this, "Closing marked connection from %1 since request body reader has completed",
            getForeignAddress());
        closeConnection(*m_markedForClosure);
    }
}

void HttpServerConnection::cleanUpOnConnectionClosure(
    SystemError::ErrorCode reason)
{
    NX_ASSERT(isInSelfAioThread());

    if (m_currentRequestBodyWriter)
        m_currentRequestBodyWriter->writeEof(reason);

    m_currentMsgBody.reset();

    m_responseQueue.clear();
}

} // namespace nx::network::http
