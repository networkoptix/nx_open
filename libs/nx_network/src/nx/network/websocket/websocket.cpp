// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket.h"

#include <nx/network/socket_global.h>

#include <nx/utils/std/future.h>

namespace nx::network::websocket {

static const auto kAliveTimeout = std::chrono::seconds(10);
static const auto kBufferSize = 4096;
static const auto kMaxIncomingMessageQueueSize = 1000;

using namespace std::placeholders;

WebSocket::WebSocket(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    SendMode sendMode,
    ReceiveMode receiveMode,
    Role role,
    FrameType frameType,
    network::websocket::CompressionType compressionType)
    :
    m_socket(std::move(streamSocket)),
    m_parser(role, std::bind(&WebSocket::gotFrame, this, _1, _2, _3)),
    m_serializer(role == Role::client),
    m_sendMode(sendMode),
    m_receiveMode(receiveMode),
    m_pingTimer(new nx::network::aio::Timer),
    m_pongTimer(new nx::network::aio::Timer),
    m_aliveTimeout(kAliveTimeout),
    m_frameType(
        frameType == FrameType::binary || frameType == FrameType::text
            ? frameType
            : FrameType::binary),
    m_compressionType(compressionType)
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().websocketConnectionCount;

    m_socket->setRecvTimeout(0);
    m_socket->setSendTimeout(0);
    bindToAioThread(m_socket->getAioThread());
    m_readBuffer.reserve(kBufferSize);
}

WebSocket::WebSocket(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    Role role,
    FrameType frameType,
    network::websocket::CompressionType compressionType)
    :
    WebSocket(
        std::move(streamSocket),
        SendMode::singleMessage,
        ReceiveMode::message,
        role,
        frameType,
        compressionType)
{
}

WebSocket::~WebSocket()
{
    pleaseStopSync();

    --SocketGlobals::instance().debugCounters().websocketConnectionCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

void WebSocket::start()
{
    m_pingTimer->start(m_aliveTimeout, [this]() { onPingTimer(); });
    m_socket->readSomeAsync(
        &m_readBuffer,
        [this](SystemError::ErrorCode error, size_t transferred)
        {
            onRead(error, transferred);
        });
}

void WebSocket::onPingTimer()
{
    m_pongTimer->start(
        m_aliveTimeout,
        [this]()
        {
            nx::utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
            onRead(SystemError::timedOut, 0);
            if (!watcher.interrupted())
                onWrite(SystemError::timedOut, 0);
        });

    if (!m_pingPongDisabled)
        sendControlRequest(FrameType::ping);

    m_pingTimer->start(m_aliveTimeout, [this]() { onPingTimer(); });
}

void WebSocket::onRead(SystemError::ErrorCode error, size_t transferred)
{
    if (m_failed)
    {
        callOnReadhandler(SystemError::connectionAbort, 0);
        return;
    }

    if (error != SystemError::noError)
    {
        setFailedState(nx::format("Read error %1", error));
        callOnReadhandler(error, 0);
        return;
    }

    if (transferred == 0)
    {
        setFailedState("0 bytes read");
        callOnReadhandler(SystemError::connectionAbort, 0);
        return;
    }

    m_parser.consume(m_readBuffer.data(), (int)transferred);
    if (m_failed) //< Might be set while parsing
    {
        callOnReadhandler(SystemError::connectionAbort, 0);
        return;
    }

    m_pongTimer->cancelSync();
    m_readBuffer.resize(0);
    m_readBuffer.reserve(kBufferSize);

    if (m_incomingMessageQueue.size() != 0 && m_userReadContext)
    {
        auto incomingMessage = m_incomingMessageQueue.popFront();
        *(m_userReadContext->bufferPtr) = incomingMessage;
        utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
        callOnReadhandler(SystemError::noError, incomingMessage.size());
        if (watcher.interrupted())
            return;
    }

    if (m_incomingMessageQueue.size() > kMaxIncomingMessageQueueSize)
    {
        NX_DEBUG(
            this,
            nx::format("Incoming message queue breached %1 messages threshold. Reading ceased")
                .args(kMaxIncomingMessageQueueSize));
        m_readingCeased = true;
        return;
    }

    m_socket->readSomeAsync(
        &m_readBuffer,
        [this](SystemError::ErrorCode error, size_t transferred)
        {
            onRead(error, transferred);
        });
}

void WebSocket::disablePingPong()
{
    m_pingPongDisabled = true;
}

void WebSocket::callOnReadhandler(SystemError::ErrorCode error, size_t transferred)
{
    if (m_userReadContext)
    {
        auto userReadContext = std::move(m_userReadContext);
        userReadContext->handler(error, transferred);
    }
}

void WebSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractAsyncChannel::bindToAioThread(aioThread);
    m_socket->bindToAioThread(aioThread);
    m_pingTimer->bindToAioThread(aioThread);
    m_pongTimer->bindToAioThread(aioThread);
}

void WebSocket::stopWhileInAioThread()
{
    m_pingTimer.reset();
    m_pongTimer.reset();
    m_socket.reset();
}

void WebSocket::setIsLastFrame()
{
    dispatch([this]() { m_isLastFrame = true; });
}

void WebSocket::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_failed)
            {
                NX_DEBUG(
                    this,
                    "readSomeAsync called after connection has been terminated. Ignoring.");
                handler(SystemError::connectionAbort, 0);
                return;
            }

            NX_ASSERT(
                !m_userReadContext,
                "Read operation has been queued before previous handler fired");

            if (m_incomingMessageQueue.size() != 0)
            {
                auto incomingMessage = m_incomingMessageQueue.popFront();
                *buffer = incomingMessage;

                utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
                handler(SystemError::noError, incomingMessage.size());
                if (watcher.interrupted())
                    return;

                if (m_readingCeased)
                {
                    m_readingCeased = false;
                    m_socket->readSomeAsync(
                        &m_readBuffer,
                        [this](SystemError::ErrorCode error, size_t transferred)
                        {
                            onRead(error, transferred);
                        });
                }

                return;
            }

            m_userReadContext.reset(new UserReadContext(std::move(handler), buffer));
        });
}

void WebSocket::sendAsync(const nx::Buffer* buffer, IoCompletionHandler handler)
{
    // TODO: #akolesnikov Avoid copying buffer into lambda.
    post(
        [this, buffer = *buffer, handler = std::move(handler)]() mutable
        {
            nx::Buffer writeBuffer;
            if (m_sendMode == SendMode::singleMessage)
            {
                writeBuffer = m_serializer.prepareMessage(buffer, m_frameType, m_compressionType);
            }
            else
            {
                FrameType type = !m_isFirstFrame ? FrameType::continuation : m_frameType;
                writeBuffer = m_serializer.prepareFrame(buffer, type, m_isLastFrame);
                m_isFirstFrame = m_isLastFrame;
                if (m_isLastFrame)
                    m_isLastFrame = false;
            }

            sendMessage(writeBuffer, buffer.size(), std::move(handler));
        });
}

void WebSocket::setAliveTimeout(std::chrono::milliseconds timeout)
{
    m_aliveTimeout = timeout;
}

void WebSocket::sendCloseAsync()
{
    post([this]() { sendControlRequest(FrameType::close); });
}

void WebSocket::sendMessage(const nx::Buffer& message, int writeSize, IoCompletionHandler handler)
{
    NX_VERBOSE(this, "SendMessage: IsFailed: %1, Write size: %2", m_failed, writeSize);
    if (m_failed)
    {
        NX_VERBOSE(
            this,
            "sendMessage() called after connection has been terminated. Ignoring.");
        handler(SystemError::connectionAbort, 0);
        return;
    }

    m_writeQueue.emplace(std::move(handler), message);
    if (m_writeQueue.size() == 1)
    {
        m_socket->sendAsync(
            &m_writeQueue.front().second,
            [this, writeSize](SystemError::ErrorCode error, size_t transferred)
            {
                onWrite(error, transferred == 0 ? 0 : writeSize);
            });
    }
}

QString WebSocket::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

void WebSocket::setFailedState(const QString& message)
{
    m_failed = true;
    m_lastErrorMessage = message;
}

void WebSocket::onWrite(SystemError::ErrorCode error, size_t transferred)
{
    if (m_failed)
    {
        while (!m_writeQueue.empty())
        {
            utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
            callOnWriteHandler(SystemError::connectionAbort, 0);
            if (watcher.interrupted())
                return;
        }
        return;
    }

    if (error != SystemError::noError)
    {
        setFailedState(nx::format("Write error %1", error));
        while (!m_writeQueue.empty())
        {
            utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
            callOnWriteHandler(error, 0);
            if (watcher.interrupted())
                return;
        }
    }
    else if (transferred == 0)
    {
        setFailedState("0 bytes written");
        while (!m_writeQueue.empty())
        {
            utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
            callOnWriteHandler(SystemError::connectionAbort, 0);
            if (watcher.interrupted())
                return;
        }
    }
    else
    {
        utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
        callOnWriteHandler(SystemError::noError, transferred);
        if (watcher.interrupted())
            return;

        if (!m_writeQueue.empty())
        {
            m_socket->sendAsync(
                &m_writeQueue.front().second,
                [this](SystemError::ErrorCode error, size_t transferred)
                {
                    onWrite(error, transferred);
                });
        }
    }
}

void WebSocket::callOnWriteHandler(SystemError::ErrorCode error, size_t transferred)
{
    const auto userWritePair = std::move(m_writeQueue.front());
    m_writeQueue.pop();
    userWritePair.first(error, transferred);
}

void WebSocket::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_pingTimer->cancelSync();
    m_pongTimer->cancelSync();
    m_socket->cancelIOSync(eventType);
}

void WebSocket::gotFrame(FrameType type, const nx::Buffer& data, bool fin)
{
    NX_VERBOSE(
        this,
        nx::format("Got frame. Type: %1, size from header: %2")
            .args(m_parser.frameType(), m_parser.frameSize()));

    if (isDataFrame(type))
    {
        m_incomingMessageQueue.append(data);
        if (m_receiveMode == ReceiveMode::frame || fin)
            m_incomingMessageQueue.lock();

        return;
    }

    switch (type)
    {
        case FrameType::ping:
            if (!m_pingPongDisabled)
                sendControlResponse(FrameType::pong);
            break;
        case FrameType::pong:
            NX_ASSERT(m_controlBuffer.empty());
            break;
        case FrameType::close:
            sendControlResponse(FrameType::close);
            setFailedState("Received 'close' control frame");
            break;
        default:
            setFailedState(nx::format("Invaild frame type %1", (int) type));
            NX_DEBUG(
                this, "%1: Got frame with invalid type %2. Going to failed state.",
                __func__, (int) type);
            break;
    }
}

void WebSocket::sendControlResponse(FrameType type)
{
    nx::Buffer responseFrame =
        m_serializer.prepareMessage(m_controlBuffer, type, m_compressionType);
    m_controlBuffer.resize(0);

    sendMessage(
        responseFrame, responseFrame.size(),
        [this, type](SystemError::ErrorCode error, size_t /*transferred*/)
        {
            NX_VERBOSE(
                this,
                nx::format("Control response %1 has been sent. Result: %2").args(
                    frameTypeString(type),
                    error));
        });
}

void WebSocket::sendControlRequest(FrameType type)
{
    nx::Buffer requestFrame = m_serializer.prepareMessage("", type, m_compressionType);
    sendMessage(
        requestFrame, requestFrame.size(),
        [this, type](SystemError::ErrorCode error, size_t /*transferred*/)
        {
            NX_VERBOSE(
                this,
                nx::format("Control request %1 has been sent. Result: %2").args(
                    frameTypeString(type),
                    error));
        });
}

std::string frameTypeString(FrameType type)
{
    switch (type)
    {
        case FrameType::continuation: return "continuation";
        case FrameType::text: return "text";
        case FrameType::binary: return "binary";
        case FrameType::close: return "close";
        case FrameType::ping: return "ping";
        case FrameType::pong: return "pong";
    }

    return "";
}

} // namespace nx::network::websocket
