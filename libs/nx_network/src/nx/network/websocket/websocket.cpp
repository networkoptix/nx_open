#include "websocket.h"

#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace websocket {

static const auto kAliveTimeout = std::chrono::seconds(10);
static const auto kBufferSize = 4096;
static const auto kMaxIncomingMessageQueueSize = 1000;

WebSocket::WebSocket(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    SendMode sendMode,
    ReceiveMode receiveMode,
    Role role,
    FrameType frameType,
    network::websocket::CompressionType compressionType)
    :
    m_socket(std::move(streamSocket)),
    m_parser(role, this, compressionType),
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
    m_socket->setRecvTimeout(0);
    m_socket->setSendTimeout(0);
    aio::AbstractAsyncChannel::bindToAioThread(m_socket->getAioThread());
    m_pingTimer->bindToAioThread(m_socket->getAioThread());
    m_pongTimer->bindToAioThread(m_socket->getAioThread());
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
            nx::utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
            onRead(SystemError::timedOut, 0);
            if (!watcher.objectDestroyed())
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
        m_failed = true;
        callOnReadhandler(error, 0);
        return;
    }

    if (transferred == 0)
    {
        m_failed = true;
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
        utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
        callOnReadhandler(SystemError::noError, incomingMessage.size());
        if (watcher.objectDestroyed())
            return;
    }

    if (m_incomingMessageQueue.size() > kMaxIncomingMessageQueueSize)
    {
        NX_DEBUG(
            this,
            lm("Incoming message queue breached %1 messages treshold. Reading ceased")
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

                utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
                handler(SystemError::noError, incomingMessage.size());
                if (watcher.objectDestroyed())
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

void WebSocket::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    post(
        [this, buffer = buffer, handler = std::move(handler)]() mutable
        {
            nx::Buffer writeBuffer;
            if (m_sendMode == SendMode::singleMessage)
            {
                writeBuffer = m_serializer.prepareMessage(buffer, m_frameType, m_compressionType);
            }
            else
            {
                FrameType type = !m_isFirstFrame ? FrameType::continuation : m_frameType;
                writeBuffer = m_serializer.prepareFrame(
                    buffer, type, m_compressionType, m_isLastFrame, m_isFirstFrame);
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
        NX_DEBUG(
            this,
            "sendMessage() called after connection has been terminated. Ignoring.");
        handler(SystemError::connectionAbort, 0);
        return;
    }

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
        return;
    }

    if (error != SystemError::noError)
    {
        m_failed = true;
        while (!m_writeQueue.empty())
        {
            utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
            callOnWriteHandler(error, 0);
            if (watcher.objectDestroyed())
                return;
        }
    }
    else if (transferred == 0)
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
    m_pongTimer->cancelSync();
    m_socket->cancelIOSync(eventType);
}

void WebSocket::gotFrame(FrameType type, const nx::Buffer& data, bool fin)
{
    NX_VERBOSE(
        this,
        lm("Got frame. Type: %1, size from header: %2")
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
            NX_ASSERT(m_controlBuffer.isEmpty());
            break;
        case FrameType::close:
            sendControlResponse(FrameType::close);
            m_failed = true;
            break;
        default:
            NX_ASSERT(false);
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
                lm("Control response %1 has been sent. Result: %2").args(
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
                lm("Control request %1 has been sent. Result: %2").args(
                    frameTypeString(type),
                    error));
        });
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
