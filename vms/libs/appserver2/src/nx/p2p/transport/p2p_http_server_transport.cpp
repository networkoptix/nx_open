// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "p2p_http_server_transport.h"

#include <nx/network/abstract_socket.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/p2p/transport/ping_headers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/system_error.h>

namespace nx::p2p {

namespace {

const int kBufferSize = 4096;

static void resetBuffer(nx::Buffer* buffer)
{
    buffer->clear();
    buffer->reserve(kBufferSize);
}

} // namespace

std::optional<std::chrono::milliseconds> P2PHttpServerTransport::s_pingTimeout = std::nullopt;
bool P2PHttpServerTransport::s_pingPongDisabled = false;

P2PHttpServerTransport::P2PHttpServerTransport(
    std::unique_ptr<network::AbstractStreamSocket> socket,
    network::websocket::FrameType messageType,
    std::optional<std::chrono::milliseconds> pingTimeout)
    :
    m_sendSocket(std::move(socket)),
    m_messageType(messageType),
    m_pingTimeout(pingTimeout)
{
    bindToAioThread(m_sendSocket->getAioThread());
    m_sendSocket->setNonBlockingMode(true);
    m_sendSocket->setRecvTimeout(0);
    m_sendSocket->setSendTimeout(0);
    m_sendBuffer.reserve(kBufferSize);
    m_sendChannelReadBuffer.reserve(kBufferSize);
    m_readBuffer.reserve(kBufferSize);
    m_httpParser.setMessage(&m_httpMessage);
}

void P2PHttpServerTransport::start(
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onGetRequestReceived)
{
    m_onGetRequestReceived = std::move(onGetRequestReceived);

    m_timer.start(
        std::chrono::seconds(10),
        [this]()
        {
            if (m_failed)
                return;

            if (m_onGetRequestReceived)
                m_onGetRequestReceived(SystemError::connectionAbort);
            else
                initiatePingPong();
        });

    m_sendSocket->readSomeAsync(
        &m_sendChannelReadBuffer,
        [this](SystemError::ErrorCode error, size_t transferred)
        {
            if (m_failed)
                return;

            utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
            m_onGetRequestReceived(
                error != SystemError::noError || transferred == 0
                    ? SystemError::connectionAbort
                    : SystemError::noError);

            if (watcher.interrupted())
                return;

            m_onGetRequestReceived = nullptr;
            if (error != SystemError::noError)
            {
                NX_ASSERT(false, "Transport is supposed to be destroyed on error");
                return;
            }

            m_sendSocket->readSomeAsync(
                &m_sendChannelReadBuffer,
                [this](SystemError::ErrorCode error, size_t transferred)
                {
                    onReadFromSendSocket(error, transferred);
                });
        });
}

void P2PHttpServerTransport::initiatePingPong()
{
    if (!pingTimeout())
    {
        NX_DEBUG(this, "Ping-pong is disabled");
        return;
    }

    if (s_pingPongDisabled)
    {
        NX_DEBUG(this, "Ping-pong is disabled for tests");
        return;
    }

    NX_DEBUG(this, "Ping-pong is enabled. Starting with %1 timeout", *pingTimeout());
    m_timer.start(
        *pingTimeout(),
        [this]()
        {
            if (m_failed)
                return;

            sendPingOrPong(Headers::ping);
            m_timer.start(
                *pingTimeout() / 2,
                [this]()
                {
                    if (m_failed)
                        return;

                    NX_DEBUG(this, "Closing connection because there was no answer to ping");
                    setFailedState(SystemError::connectionAbort,
                        "Closing connection because there was no answer to ping");
                });
        });
}

void P2PHttpServerTransport::sendPingOrPong(Headers type)
{
    post(
        [this, type]()
        {
            if (m_failed)
                return;

            auto handler =
                [this, type](SystemError::ErrorCode code, size_t transferred)
                {
                    NX_VERBOSE(
                        this, "%1 sent. code: %2, transferred: %3",
                        (type == Headers::ping ? "ping" : "pong"), code, transferred);
                };

            m_outgoingMessageQueue.push(OutgoingData{
                std::nullopt, std::move(handler), type});

            sendNextMessage();
        });
}

void P2PHttpServerTransport::setPingPongDisabled(bool value)
{
    s_pingPongDisabled = value;
}

std::optional<std::chrono::milliseconds> P2PHttpServerTransport::pingTimeout() const
{
    if (s_pingTimeout)
        return s_pingTimeout;

    return m_pingTimeout;
}

void P2PHttpServerTransport::setPingTimeout(std::optional<std::chrono::milliseconds> pingTimeout)
{
    s_pingTimeout = pingTimeout;
}

void P2PHttpServerTransport::setFailedState(
    SystemError::ErrorCode errorCode,
    const QString& message)
{
    NX_DEBUG(this, "Going to failed state");
    m_failed = true;
    m_lastErrorMessage = message;
    if (m_userReadHandlerPair)
    {
        auto userReadHandlerPair = std::move(m_userReadHandlerPair);
        userReadHandlerPair->second(errorCode, 0);
    }
}

void P2PHttpServerTransport::onReadFromSendSocket(
    SystemError::ErrorCode error,
    size_t transferred)
{
    if (error != SystemError::noError || transferred == 0)
    {
        setFailedState(error, NX_FMT("Reading from send socket failed. "
            "Error: %1, transferred: %2", error, transferred));
    }
    else
    {
        resetBuffer(&m_sendChannelReadBuffer);
        m_sendSocket->readSomeAsync(
            &m_sendChannelReadBuffer,
            [this](SystemError::ErrorCode error, size_t transferred)
            {
                if (m_failed)
                    return;

                onReadFromSendSocket(error, transferred);
            });
    }
}

P2PHttpServerTransport::~P2PHttpServerTransport()
{
    pleaseStopSync();
}

void P2PHttpServerTransport::onRead(SystemError::ErrorCode error, size_t transferred)
{
    if (error != SystemError::noError || transferred <= 0)
    {
        NX_DEBUG(
            this, "%1: failed. Error: %2, transferred: %3",
            __func__, error, transferred);
        setFailedState(SystemError::connectionAbort,
            NX_FMT("Reading from read socket failed with error %1. Bytes read = %2", error, transferred));
        return;
    }

    NX_VERBOSE(this, "%1: Read %2 bytes", __func__, transferred);
    size_t bytesProcessed = 0;
    while (true)
    {
        auto state = m_httpParser.parse(m_readBuffer, &bytesProcessed);
        NX_VERBOSE(this, "%1. Parsing state: %2", __func__, state);
        switch (state)
        {
            case nx::network::server::ParserState::readingBody:
            case nx::network::server::ParserState::readingMessage:
            {
                m_readBuffer.erase(0, bytesProcessed);
                break;
            }
            case nx::network::server::ParserState::done:
            {
                network::http::Request httpRequest;
                httpRequest.headers = m_httpMessage.headers();
                httpRequest.messageBody = m_httpParser.fetchMessageBody();
                onIncomingPost(std::move(httpRequest));
                m_httpParser.reset();
                m_readBuffer.erase(0, bytesProcessed);
                break;
            }
            case nx::network::server::ParserState::init:
            {
                NX_ASSERT(false);
                break;
            }
            case nx::network::server::ParserState::failed:
            {
                setFailedState(SystemError::connectionAbort, "Failed to parse incoming http message");
                return;
            }
        }

        if (m_readBuffer.size() == 0 || bytesProcessed == 0)
            break;
    }

    m_readSocket->readSomeAsync(
        &m_readBuffer,
        [this](auto error, auto transferred)
        {
            if (m_failed)
                return;

            onRead(error, transferred);
        });
}

void P2PHttpServerTransport::gotPostConnection(
    std::unique_ptr<network::AbstractStreamSocket> socket,
    nx::network::http::Request request)
{
    post(
        [this, socket = std::move(socket), request = std::move(request)]() mutable
        {
            if (m_failed)
                return;

            m_readSocket = std::move(socket);
            m_readSocket->setNonBlockingMode(true);
            m_readSocket->bindToAioThread(getAioThread());
            m_readSocket->setRecvTimeout(0);

            NX_VERBOSE(this, "Got post connection");
            onIncomingPost(std::move(request));
            m_readSocket->readSomeAsync(
                &m_readBuffer,
                [this](auto error, auto transferred)
                {
                    if (m_failed)
                        return;

                    onRead(error, transferred);
                });
        });
}

void P2PHttpServerTransport::readSomeAsync(
    nx::Buffer* const buffer,
    network::IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_failed)
                return;

            if (m_onGetRequestReceived)
                return handler(SystemError::connectionAbort, 0);

            NX_ASSERT(!m_userReadHandlerPair);
            m_userReadHandlerPair = std::make_unique<std::pair<nx::Buffer* const, network::IoCompletionHandler>>(
                std::make_pair(buffer, std::move(handler)));
        });
}

void P2PHttpServerTransport::onIncomingPost(nx::network::http::Request request)
{
    bool pingOrPong = false;
    if (request.headers.contains(kPingHeaderName))
    {
        NX_VERBOSE(this, "Ping received. Sending pong");
        sendPingOrPong(Headers::pong);
        pingOrPong = true;
    }

    if (request.headers.contains(kPongHeaderName))
    {
        NX_VERBOSE(this, "Pong received");
        pingOrPong = true;
    }

    if (!pingOrPong)
    {
        sendPostResponse(
            [this, request = std::move(request)]
            (SystemError::ErrorCode error, size_t transferred)
            {
                if (m_failed)
                    return;

                if (error != SystemError::noError || transferred <= 0)
                {
                    NX_VERBOSE(this, "Response to POST failed");
                    setFailedState(error, NX_FMT(
                        "Response to POST failed, errorCode: %1, bytes transfered: %2",
                        error, transferred));
                }
                else if (NX_ASSERT(m_userReadHandlerPair))
                {
                    NX_VERBOSE(this, "Response to POST succeeded");
                    UserReadHandlerPair userReadHandlerPair;
                    userReadHandlerPair.swap(m_userReadHandlerPair);

                    *userReadHandlerPair->first = m_messageType == network::websocket::FrameType::binary
                        ? nx::utils::fromBase64(request.messageBody)
                        : request.messageBody;

                    userReadHandlerPair->second(SystemError::noError, userReadHandlerPair->first->size());
                }
            });
    }
    else
    {
        sendPostResponse(
            [this](SystemError::ErrorCode error, size_t transferred)
            {
                NX_VERBOSE(
                    this, "Response to POST (ping or pong). Error: %1, transferred: %2",
                    error, transferred);

                if (error != SystemError::noError || transferred <= 0)
                {
                    setFailedState(error, NX_FMT("Response to POST (ping or pong) failed. "
                        "Error: %1, transferred: %2", error, transferred));
                }
            });
    }

    initiatePingPong();
}

void P2PHttpServerTransport::addDateHeader(network::http::HttpHeaders* headers)
{
    using namespace std::chrono;
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
    headers->emplace("Date", network::http::formatDateTime(dateTime));
}

void P2PHttpServerTransport::sendPostResponse(network::IoCompletionHandler handler)
{
    NX_VERBOSE(this, "Sending response to POST.");
    network::http::Response response;
    response.statusLine.statusCode = network::http::StatusCode::ok;
    response.statusLine.version = network::http::http_1_1;
    response.statusLine.reasonPhrase = "Ok";

    response.headers.emplace("Content-Length", "0");
    response.headers.emplace("Connection", "keep-alive");
    addDateHeader(&response.headers);

    response.serialize(&m_responseBuffer);

    m_readSocket->sendAsync(
        &m_responseBuffer,
        [this, handler = std::move(handler)]
        (SystemError::ErrorCode error, size_t transferred)
        {
            if (m_failed)
                return;

            m_responseBuffer.clear();
            handler(error, transferred);
        });
}

void P2PHttpServerTransport::sendNextMessage()
{
    if (m_sendInProgress || m_outgoingMessageQueue.empty())
    {
        NX_VERBOSE(
            this, "%1: Can't initiate send. in progress: %2, message queue size: %3",
            __func__, m_sendInProgress, m_outgoingMessageQueue.size());

        return;
    }

    auto next = std::move(m_outgoingMessageQueue.front());
    m_outgoingMessageQueue.pop();
    if (m_onGetRequestReceived || m_failed)
    {
        NX_DEBUG(this, "%1: Transport is in the failed state. Can't send anything", __func__);
        next.handler(SystemError::connectionAbort, 0);
        return;
    }

    if (m_firstSend)
    {
        m_sendBuffer = makeInitialResponse();
        m_firstSend = false;
    }

    if (next.buffer)
    {
        nx::utils::append(
            &m_sendBuffer, makeFrameHeader(Headers::contentType, next.buffer->size() + 2),
            *next.buffer, "\r\n");
    }
    else
    {
        nx::utils::append(&m_sendBuffer, makeFrameHeader(next.headerMask, /*length*/ 0));
    }

    NX_VERBOSE(
        this, "%1: Sending next message. Queue size: %2",
        __func__, m_outgoingMessageQueue.size());

    m_sendInProgress = true;
    m_sendSocket->sendAsync(
        &m_sendBuffer,
        [this, next = std::move(next)](SystemError::ErrorCode error, size_t transferred)
        {
            if (m_failed)
                return;

            NX_VERBOSE(
                this,
                nx::format("Send completed. error: %1, transferred: %2").args(error, transferred));

            if (error != SystemError::noError || transferred == 0)
            {
                setFailedState(error,
                    NX_FMT("Sending data via send socket failed with error %1", error));
            }

            resetBuffer(&m_sendBuffer);
            utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
            next.handler(error, transferred);
            if (watcher.interrupted())
                return;

            m_sendInProgress = false;
            sendNextMessage();
        });
}

void P2PHttpServerTransport::sendAsync(
    const nx::Buffer* buffer,
    network::IoCompletionHandler handler)
{
    nx::Buffer encodedBuffer =
        m_messageType == network::websocket::FrameType::binary
        ? nx::utils::toBase64(*buffer)
        : *buffer;

    post(
        [this, encodedBuffer = std::move(encodedBuffer), handler = std::move(handler)]() mutable
        {
            if (m_failed)
                return;

            m_outgoingMessageQueue.push(OutgoingData{
                encodedBuffer, std::move(handler), Headers::contentType});

            sendNextMessage();
        });
}

nx::Buffer P2PHttpServerTransport::makeInitialResponse() const
{
    network::http::Response initialResponse;
    initialResponse.statusLine.statusCode = network::http::StatusCode::ok;
    initialResponse.statusLine.reasonPhrase = "Ok";
    initialResponse.statusLine.version = network::http::http_1_1;

    auto& headers = initialResponse.headers;
    headers.emplace("Host", m_sendSocket->getForeignHostName());
    headers.emplace("Content-Type", "multipart/mixed; boundary=ec2boundary");
    headers.emplace("Access-Control-Allow-Origin", "*");
    headers.emplace("Connection", "Keep-Alive");
    addDateHeader(&headers);

    nx::Buffer result;
    initialResponse.serialize(&result);

    return result;
}

nx::Buffer P2PHttpServerTransport::makeFrameHeader(int headers, int length) const
{
    nx::Buffer headerBuffer;

    if (headers & Headers::contentType)
    {
        network::http::serializeHeaders(
            {network::http::HttpHeader(
                "Content-Type",
                m_messageType == network::websocket::FrameType::text
                    ? "application/json" : "application/ubjson")},
            &headerBuffer);
    }

    if (headers & Headers::ping)
    {
        network::http::serializeHeaders(
            {network::http::HttpHeader(kPingHeaderName, "ping")}, &headerBuffer);
    }

    if (headers & Headers::pong)
    {
        network::http::serializeHeaders(
            {network::http::HttpHeader(kPingHeaderName, "pong")}, &headerBuffer);
    }

    network::http::serializeHeaders(
        {network::http::HttpHeader("Content-Length", std::to_string(length))}, &headerBuffer);

    return nx::utils::buildString<nx::Buffer>("--ec2boundary\r\n", headerBuffer, "\r\n");
}

void P2PHttpServerTransport::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    BasicPollable::bindToAioThread(aioThread);
    m_sendSocket->bindToAioThread(aioThread);
    if (m_readSocket)
        m_readSocket->bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

void P2PHttpServerTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_sendSocket->cancelIOSync(eventType);
    if (m_readSocket)
        m_readSocket->cancelIOSync(eventType);
}

network::aio::AbstractAioThread* P2PHttpServerTransport::getAioThread() const
{
    return BasicPollable::getAioThread();
}

network::SocketAddress P2PHttpServerTransport::getForeignAddress() const
{
    return m_sendSocket->getForeignAddress();
}

void P2PHttpServerTransport::stopWhileInAioThread()
{
    m_timer.cancelSync();
    m_sendSocket.reset();
    m_readSocket.reset();
}

QString P2PHttpServerTransport::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

} // namespace nx::network
