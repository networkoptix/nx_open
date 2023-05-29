// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "p2p_http_client_transport.h"

#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <nx/network/http/custom_headers.h>
#include "nx/p2p/transport/ping_headers.h"
#include "nx/utils/scope_guard.h"
#include "nx/utils/system_error.h"

namespace nx::p2p {

namespace {

#if defined (_DEBUG)
    static const int kMaxMessageQueueSize = 5;
#else
    static const int kMaxMessageQueueSize = 500;
#endif

} // namespace

std::optional<std::chrono::milliseconds> P2PHttpClientTransport::s_pingTimeout = std::nullopt;
bool P2PHttpClientTransport::s_pingPongDisabled = false;

P2PHttpClientTransport::P2PHttpClientTransport(
    HttpClientPtr readHttpClient,
    const nx::String& connectionGuid,
    network::websocket::FrameType messageType,
    const nx::utils::Url& url,
    std::optional<std::chrono::milliseconds> pingTimeout)
    :
    m_writeHttpClient(new network::http::AsyncClient({readHttpClient->adapterFunc()})),
    m_readHttpClient(std::move(readHttpClient)),
    m_messageType(messageType),
    m_url(url),
    m_connectionGuid(connectionGuid),
    m_pingTimeout(pingTimeout)
{
    using namespace std::chrono_literals;

    m_readHttpClient->setResponseReadTimeout(0ms);
    m_readHttpClient->setMessageBodyReadTimeout(0ms);
    const auto keepAliveOptions =
        nx::network::KeepAliveOptions(std::chrono::minutes(1), std::chrono::seconds(10), 5);
    m_readHttpClient->setKeepAlive(keepAliveOptions);
    network::http::HttpHeaders additionalHeaders;
    additionalHeaders.emplace(Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_connectionGuid);
    m_readHttpClient->setAdditionalHeaders(additionalHeaders);

    bindToAioThread(m_readHttpClient->getAioThread());
    m_writeHttpClient->setCredentials(m_readHttpClient->credentials());

    initiatePingPong();
}

void P2PHttpClientTransport::start(utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart)
{
    post(
        [this, onStart = std::move(onStart)]() mutable
        {
            m_onStartHandler = std::move(onStart);
            startReading();
        });
}

void P2PHttpClientTransport::setPingTimeout(std::optional<std::chrono::milliseconds> pingTimeout)
{
    s_pingTimeout = pingTimeout;
}

void P2PHttpClientTransport::setPingPongDisabled(bool value)
{
    s_pingPongDisabled = value;
}

std::optional<std::chrono::milliseconds> P2PHttpClientTransport::pingTimeout() const
{
    if (s_pingTimeout)
        return s_pingTimeout;

    return m_pingTimeout;
}

void P2PHttpClientTransport::sendPingOrPong(const std::string& name)
{
    NX_ASSERT(name == "ping" || name == "pong");
    post(
        [this, name]() mutable
        {
            NX_VERBOSE(this, "Sending %1", name);
            m_outgoingMessageQueue.push(OutgoingData{
                std::nullopt,
                [name, this](SystemError::ErrorCode error, size_t transferred)
                {
                    NX_VERBOSE(
                        this, "%1 sent. Error: %2, transferred: %3",
                        name, error, transferred);
                },
                network::http::HttpHeaders{}});

            m_outgoingMessageQueue.back().headers.emplace(Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_connectionGuid);
            if (name == "ping")
                m_outgoingMessageQueue.back().headers.emplace(kPingHeaderName, name);
            else
                m_outgoingMessageQueue.back().headers.emplace(kPongHeaderName, name);

            sendNextMessage();
        });
}

void P2PHttpClientTransport::setFailedState()
{
    m_failed = true;
    if (m_userReadHandlerPair)
        m_userReadHandlerPair->second(SystemError::timedOut, 0);
}

void P2PHttpClientTransport::initiatePingPong()
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
            sendPingOrPong("ping");
            m_timer.start(
                *pingTimeout() / 2,
                [this]()
                {
                    NX_DEBUG(this, "Closing connection because there was no answer to ping");
                    setFailedState();
                });
        });
}

P2PHttpClientTransport::~P2PHttpClientTransport()
{
    pleaseStopSync();
}

void P2PHttpClientTransport::stopWhileInAioThread()
{
    m_timer.cancelSync();
    m_writeHttpClient.reset();
    m_readHttpClient.reset();
}

void P2PHttpClientTransport::readSomeAsync(
    nx::Buffer* const buffer,
    network::IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            NX_ASSERT(
                !m_userReadHandlerPair,
                "Don't call readSomeAsync() again, before the previous handler has been invoked.");

            if (m_userReadHandlerPair)
                return handler(SystemError::notSupported, 0);

            if (!m_incomingMessageQueue.empty())
            {
                if (m_failed)
                {
                    NX_VERBOSE(
                        this,
                        "The connection is in a failed state, but we have some"
                        " buffered messages left. Handing them out");
                }

                const auto queueBuffer = m_incomingMessageQueue.front();
                m_incomingMessageQueue.pop();
                stopOrResumeReaderWhileInAioThread();
                buffer->append(m_messageType == network::websocket::FrameType::binary
                    ? nx::utils::fromBase64(queueBuffer) : queueBuffer);
                handler(SystemError::noError, queueBuffer.size());
                return;
            }

            if (m_failed)
            {
                NX_VERBOSE(this, nx::format("The connection has failed. Reporting with a read handler"));
                handler(SystemError::connectionAbort, 0);
                return;
            }

            // No incoming message in the queue.
            m_userReadHandlerPair.reset(
                new std::pair<nx::Buffer* const, network::IoCompletionHandler>(
                    buffer,
                    std::move(handler)));
        });
}

void P2PHttpClientTransport::sendNextMessage()
{
    if (m_sendInProgress || m_outgoingMessageQueue.empty())
    {
        NX_VERBOSE(
            this, "%1: Can't intiate send. in progress: %2, message queue size: %3",
            __func__, m_sendInProgress, m_outgoingMessageQueue.size());

        return;
    }

    auto next = std::move(m_outgoingMessageQueue.front());
    m_outgoingMessageQueue.pop();
    if (m_failed)
    {
        NX_DEBUG(this, "%1: Transport is in the failed state. Can't send anything", __func__);
        next.handler(SystemError::connectionAbort, 0);
        return;
    }

    NX_VERBOSE(
        this, "%1: Sending message. Queue size: %2",
        __func__, m_outgoingMessageQueue.size());

    if (next.buffer)
    {
        m_writeHttpClient->setRequestBody(std::make_unique<PostBodySource>(
            m_messageType,
            m_messageType == network::websocket::FrameType::binary
                ? nx::utils::toBase64(*next.buffer) : *next.buffer));
    }

    next.headers.emplace(Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_connectionGuid);
    m_writeHttpClient->setAdditionalHeaders(next.headers);

    NX_VERBOSE(this, "%1: Sending POST request to %2", __func__, m_url);
    m_sendInProgress = true;
    m_writeHttpClient->doPost(
        m_url,
        [this, next = std::move(next)]()
        {
            const bool isResponseValid = m_writeHttpClient->response()
                && m_writeHttpClient->response()->statusLine.statusCode
                    == network::http::StatusCode::ok;

            NX_VERBOSE(
                this, "%1: Received response to POST from %2. Result: %3",
                __func__, m_url, isResponseValid);

            const auto resultCode = !m_writeHttpClient->failed() && isResponseValid
                ? SystemError::noError : SystemError::connectionAbort;

            std::size_t transferred = 0;
            if (next.buffer && resultCode == SystemError::noError)
                transferred = next.buffer->size();

            if (m_writeHttpClient->failed() || !isResponseValid)
                setFailedState();

            utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
            next.handler(resultCode, transferred);
            if (watcher.interrupted())
                return;

            m_sendInProgress = false;
            sendNextMessage();
        });
}

void P2PHttpClientTransport::sendAsync(
    const nx::Buffer* buffer,
    network::IoCompletionHandler handler)
{
    post(
        [this, buffer = *buffer, bufferSize = buffer->size(),
            handler = std::move(handler)]() mutable
        {
            m_outgoingMessageQueue.push(OutgoingData{
                buffer, std::move(handler), network::http::HttpHeaders{}});

            sendNextMessage();
        });
}

void P2PHttpClientTransport::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    BasicPollable::bindToAioThread(aioThread);
    m_readHttpClient->bindToAioThread(aioThread);
    m_writeHttpClient->bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

void P2PHttpClientTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_timer.cancelSync();
    m_readHttpClient->socket()->cancelIOSync(eventType);
    m_writeHttpClient->socket()->cancelIOSync(eventType);
}

network::aio::AbstractAioThread* P2PHttpClientTransport::getAioThread() const
{
    return BasicPollable::getAioThread();
}

network::SocketAddress P2PHttpClientTransport::getForeignAddress() const
{
    return m_readHttpClient->executeInAioThreadSync(
        [this]() { return m_readHttpClient->socket()->getForeignAddress(); });
}

void P2PHttpClientTransport::stopOrResumeReaderWhileInAioThread()
{
    if (m_incomingMessageQueue.size() > kMaxMessageQueueSize)
    {
        NX_DEBUG(
            this, "Incoming message queue overflow detected (%1 pending)",
            m_incomingMessageQueue.size());
        m_readHttpClient->stopReading();
    }
    else if (
        m_incomingMessageQueue.size() < kMaxMessageQueueSize / 2
        && !m_readHttpClient->isReading())
    {
        m_readHttpClient->resumeReading();
    }
}

void P2PHttpClientTransport::startReading()
{
    m_readHttpClient->setOnResponseReceived(
        [this]()
        {
            auto nextFilter = nx::utils::bstream::makeCustomOutputStream(
                [this](const nx::ConstBufferRefType& data)
                {
                    utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
                    const auto onExit = nx::utils::makeScopeGuard(
                        [&]()
                        {
                            if (watcher.interrupted())
                                return;

                            initiatePingPong();
                        });

                    NX_VERBOSE(
                        this, "Got next multipart frame. Headers: %1",
                        m_multipartContentParser.prevFrameHeaders());

                    if (m_multipartContentParser.prevFrameHeaders().contains(kPingHeaderName))
                    {
                        NX_DEBUG(this, "Ping received, sending pong");
                        sendPingOrPong("pong");
                        return;
                    }

                    if (m_multipartContentParser.prevFrameHeaders().contains(kPongHeaderName))
                    {
                        NX_DEBUG(this, "Pong received");
                        return;
                    }

                    stopOrResumeReaderWhileInAioThread();
                    if (!m_userReadHandlerPair)
                    {
                        m_incomingMessageQueue.push(nx::Buffer(data));
                    }
                    else
                    {
                        m_userReadHandlerPair->first->append(
                            m_messageType == network::websocket::FrameType::binary
                            ? nx::utils::fromBase64(data)
                            : data);

                        m_userReadHandlerPair->second(SystemError::noError, data.size());
                        if (watcher.interrupted())
                            return;

                        m_userReadHandlerPair.reset();
                    }
                });

            const auto statusCode = m_readHttpClient->response()->statusLine.statusCode;
            NX_VERBOSE(this,
                "startReading: Received response to initial GET request to '%1', statusCode=%2",
                m_url, statusCode);

            if (nx::network::http::StatusCode::isSuccessCode(statusCode))
            {
                m_multipartContentParser.setNextFilter(nextFilter);
                const auto& headers = m_readHttpClient->response()->headers;
                const auto contentTypeIt = headers.find("Content-Type");

                NX_ASSERT(contentTypeIt != headers.end());
                if (contentTypeIt == headers.end() ||
                    !m_multipartContentParser.setContentType(contentTypeIt->second))
                {
                    NX_WARNING(
                        this, "startReading: Expected a multipart response from '%1'. It is not.", m_url);
                    m_failed = true;
                }
            }
            else
            {
                m_failed = true;
            }

            if (m_onStartHandler)
            {
                utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
                const auto resultCode =
                    m_failed ? SystemError::connectionAbort : SystemError::noError;
                nx::utils::swapAndCall(m_onStartHandler, resultCode);
                if (watcher.interrupted())
                    return;
            }

            if (m_failed)
                NX_VERBOSE(this, "startReading: Connection to '%1' aborted", m_url);
            else
                NX_VERBOSE(this, "startReading: Connection to '%1' established", m_url);

        });

    m_readHttpClient->setOnSomeMessageBodyAvailable(
        [this]()
        {
            if (!m_multipartContentParser.processData(m_readHttpClient->fetchMessageBodyBuffer()))
                m_failed = true;
        });

    m_readHttpClient->setOnDone(
        [this]()
        {
            NX_VERBOSE(
                this,
                "The read (GET) http client emitted 'onDone'. Moving to a failed state.");

            m_failed = true;
            if (m_userReadHandlerPair)
            {
                nx::Buffer outBuffer;
                SystemError::ErrorCode errorCode = SystemError::noError;

                if (!m_incomingMessageQueue.empty())
                {
                    outBuffer = m_incomingMessageQueue.front();
                    m_incomingMessageQueue.pop();
                    m_userReadHandlerPair->first->append(
                        m_messageType == network::websocket::FrameType::binary
                        ? nx::utils::fromBase64(outBuffer) : outBuffer);

                }
                else
                {
                    errorCode = SystemError::connectionAbort;
                }

                utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
                m_userReadHandlerPair->second(errorCode, outBuffer.size());
                if (watcher.interrupted())
                    return;

                m_userReadHandlerPair.reset();
            }

        if (m_onStartHandler)
            nx::utils::swapAndCall(m_onStartHandler, SystemError::connectionAbort);
        });

    NX_VERBOSE(this, "startReading: Sending initial GET request to '%1'", m_url);
    m_readHttpClient->doGet(m_url);
}

P2PHttpClientTransport::PostBodySource::PostBodySource(
    network::websocket::FrameType messageType,
    const Buffer& data)
    :
    m_messageType(messageType),
    m_data(data)
{
}

std::string P2PHttpClientTransport::PostBodySource::mimeType() const
{
    if (m_messageType == network::websocket::FrameType::text)
        return "application/json";
    return "application/ubjson";
}

std::optional<uint64_t> P2PHttpClientTransport::PostBodySource::contentLength() const
{
    return m_data.size();
}

void P2PHttpClientTransport::PostBodySource::readAsync(CompletionHandler completionHandler)
{
    completionHandler(SystemError::noError, m_data);
}

} // namespace nx::network
