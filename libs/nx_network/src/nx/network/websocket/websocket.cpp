#include "websocket.h"

#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace websocket {

static const auto kAliveTimeout = std::chrono::seconds(100);
static const auto kDefaultPingTimeoutMultiplier = 0.5;

WebSocket::WebSocket(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    SendMode sendMode,
    ReceiveMode receiveMode,
    Role role,
    FrameType frameType)
    :
    m_socket(std::move(streamSocket)),
    m_parser(role, this),
    m_serializer(role == Role::client),
    m_sendMode(sendMode),
    m_receiveMode(receiveMode),
    m_pingTimer(new nx::network::aio::Timer),
    m_aliveTimeout(kAliveTimeout),
    m_frameType(
        frameType == FrameType::binary || frameType == FrameType::text
            ? frameType
            : FrameType::binary)
{
    m_socket->setRecvTimeout(0);
    m_socket->setSendTimeout(0);
    aio::AbstractAsyncChannel::bindToAioThread(m_socket->getAioThread());
    m_pingTimer->bindToAioThread(m_socket->getAioThread());
    m_readBuffer.reserve(4096);
}

WebSocket::WebSocket(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    FrameType frameType)
    :
    WebSocket(
        std::move(streamSocket),
        SendMode::singleMessage,
        ReceiveMode::message,
        Role::undefined,
        frameType)
{
}

WebSocket::~WebSocket()
{
    pleaseStopSync();
}

void WebSocket::start()
{
    m_pingTimer->start( pingTimeout(), [this]() { onPingTimer(); });
    m_socket->readSomeAsync(
        &m_readBuffer,
        [this](SystemError::ErrorCode error, size_t transferred)
        {
            onRead(error, transferred);
        });
}

void WebSocket::onPingTimer()
{
    sendControlRequest(FrameType::ping);
    m_pingTimer->start(pingTimeout(), [this]() { onPingTimer(); });
}

void WebSocket::onRead(SystemError::ErrorCode ecode, size_t transferred)
{
    if (m_failed)
    {
        if (m_userReadPair)
            callOnReadhandler(SystemError::connectionAbort, 0);
        return;
    }

    if (ecode != SystemError::noError || transferred == 0)
    {
        m_failed = true;
        if (m_userReadPair)
            callOnReadhandler(SystemError::connectionAbort, 0);
        return;
    }

    m_parser.consume(m_readBuffer.data(), (int)transferred);
    if (m_failed) //< Might be set while parsing
    {
        if (m_userReadPair)
            callOnReadhandler(SystemError::connectionAbort, 0);
        return;
    }

    m_readBuffer.resize(0);
    m_readBuffer.reserve(4096);

    if (m_incomingMessageQueue.readySize() != 0 && m_userReadPair)
    {
        const auto incomingMessage = m_incomingMessageQueue.popFront();
        *(m_userReadPair->second) = incomingMessage;
        utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
        callOnReadhandler(SystemError::noError, incomingMessage.size());
        if (watcher.objectDestroyed())
            return;
        m_userReadPair.reset();
    }

    m_socket->readSomeAsync(
        &m_readBuffer,
        [this](SystemError::ErrorCode error, size_t transferred)
        {
            onRead(error, transferred);
        });
}

void WebSocket::callOnReadhandler(SystemError::ErrorCode error, size_t transferred)
{
    auto cb = std::move(m_userReadPair->first);
    cb(error, transferred);
}

void WebSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractAsyncChannel::bindToAioThread(aioThread);
    m_socket->bindToAioThread(aioThread);
    m_pingTimer->bindToAioThread(aioThread);
}

std::chrono::milliseconds WebSocket::pingTimeout() const
{
    auto timeoutMs = (int64_t)(m_aliveTimeout.count() * kDefaultPingTimeoutMultiplier);
    return std::chrono::milliseconds(timeoutMs);
}

void WebSocket::stopWhileInAioThread()
{
    m_pingTimer.reset();
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
                !m_userReadPair,
                "Read operation has been queued before previous handler fired");

            if (m_incomingMessageQueue.readySize() != 0)
            {
                const auto incomingMessage = m_incomingMessageQueue.popFront();
                *buffer = incomingMessage;
                utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
                handler(SystemError::noError, incomingMessage.size());
                return;
            }

            m_userReadPair.reset(new UserReadPair(std::move(handler), buffer));
        });
}

void WebSocket::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            if (m_failed)
            {
                NX_DEBUG(
                    this,
                    "sendAsync called after connection has been terminated. Ignoring.");
                handler(SystemError::connectionAbort, 0);
                return;
            }

            nx::Buffer writeBuffer;
            if (m_sendMode == SendMode::singleMessage)
            {
                m_serializer.prepareMessage(buffer, m_frameType, &writeBuffer);
            }
            else
            {
                FrameType type = !m_isFirstFrame ? FrameType::continuation : m_frameType;

                m_serializer.prepareFrame(buffer, type, m_isLastFrame, &writeBuffer);
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
    m_writeQueue.emplace(std::move(handler), message);
    if (m_writeQueue.size() == 1)
    {
        m_socket->sendAsync(
            m_writeQueue.front().second,
            [this, writeSize](SystemError::ErrorCode error, size_t transferred)
            {
                onWrite(error, transferred == 0 ? 0 : writeSize);
            });
    }
}

void WebSocket::onWrite(SystemError::ErrorCode error, size_t transferred)
{
    if (m_failed)
    {
        while (!m_writeQueue.empty())
        {
            utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
            callOnWriteHandler(SystemError::connectionAbort, 0);
            if (watcher.objectDestroyed())
                return;
        }
    }

    if (error != SystemError::noError || transferred == 0)
    {
        m_failed = true;
        while (!m_writeQueue.empty())
        {
            utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
            callOnWriteHandler(SystemError::connectionAbort, 0);
            if (watcher.objectDestroyed())
                return;
        }
    }
    else
    {
        utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
        callOnWriteHandler(SystemError::noError, transferred);
        if (watcher.objectDestroyed())
            return;

        if (!m_writeQueue.empty())
        {
            m_socket->sendAsync(
                m_writeQueue.front().second,
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
    m_socket->cancelIOSync(eventType);
}

bool WebSocket::isDataFrame() const
{
    return m_parser.frameType() == FrameType::binary
        || m_parser.frameType() == FrameType::text
        || m_parser.frameType() == FrameType::continuation;
}

void WebSocket::frameStarted(FrameType /*type*/, bool /*fin*/)
{
    NX_VERBOSE(this, lm("Frame started. Type: %1, size from header: %2").args(
        m_parser.frameType(), m_parser.frameSize()));
}

void WebSocket::framePayload(const char* data, int len)
{
    if (!isDataFrame())
    {
        m_controlBuffer.append(data, len);
        return;
    }

    m_incomingMessageQueue.append(data, len);
    if (m_receiveMode == ReceiveMode::stream)
        m_incomingMessageQueue.lock();
}

void WebSocket::frameEnded()
{
    NX_VERBOSE(this, lm("Frame ended. Type: %1, size from header: %2").args(
        m_parser.frameType(),
        m_parser.frameSize()));

    if (m_parser.frameType() == FrameType::ping)
    {
        NX_VERBOSE(this, "Ping received.");
        sendControlResponse(FrameType::pong);
        return;
    }

    if (m_parser.frameType() == FrameType::pong)
    {
        NX_VERBOSE(this, "Pong received.");
        NX_ASSERT(m_controlBuffer.isEmpty());
        return;
    }

    if (m_parser.frameType() == FrameType::close)
    {
        NX_DEBUG(this, "Received close request, responding and terminating.");
        m_failed = true;
        sendControlResponse(FrameType::close);
        return;
    }

    if (!isDataFrame())
    {
        NX_WARNING(this, lm("Unknown frame %1").arg(m_parser.frameType()));
        m_controlBuffer.resize(0);
        return;
    }

    if (m_receiveMode != ReceiveMode::frame)
        return;

    m_incomingMessageQueue.lock();
}

void WebSocket::sendControlResponse(FrameType type)
{
    nx::Buffer responseFrame;
    m_serializer.prepareMessage(m_controlBuffer, type, &responseFrame);
    m_controlBuffer.resize(0);

    sendMessage(
        responseFrame,
        responseFrame.size(),
        [this, type](SystemError::ErrorCode error, size_t /*transferred*/)
        {
            NX_VERBOSE(
                this,
                lm("Control response %1 has been sent. Result: %2").args(
                    frameTypeString(type),
                    error));
        });
}

void WebSocket::sendControlRequest(FrameType type)
{
    nx::Buffer requestFrame;
    m_serializer.prepareMessage("", type, &requestFrame);

    sendMessage(
        requestFrame,
        requestFrame.size(),
        [this, type](SystemError::ErrorCode error, size_t /*transferred*/)
        {
            NX_VERBOSE(
                this,
                lm("Control request %1 has been sent. Result: %2").args(
                    frameTypeString(type),
                    error));
        });
}

void WebSocket::messageEnded()
{
    if (!isDataFrame())
    {
        NX_VERBOSE(this, lm("Message ended, not data frame, type: %1").arg(m_parser.frameType()));
        return;
    }

    if (m_receiveMode != ReceiveMode::message)
    {
        NX_VERBOSE(this, "Message ended, wrong receive mode.");
        return;
    }

    NX_VERBOSE(this, "Message ended, LOCKING user data.");
    m_incomingMessageQueue.lock();
}

void WebSocket::handleError(Error err)
{
    NX_DEBUG(this, lm("Parse error %1. Closing connection.").arg((int) err));
    m_failed = true;
}

QString frameTypeString(FrameType type)
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

} // namespace websocket
} // namespace network
} // namespace nx
