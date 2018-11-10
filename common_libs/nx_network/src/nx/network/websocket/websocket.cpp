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
    m_isLastFrame(false),
    m_isFirstFrame(true),
    m_pingTimer(new nx::network::aio::Timer),
    m_aliveTimer(new nx::network::aio::Timer),
    m_aliveTimeout(kAliveTimeout),
    m_lastError(SystemError::noError),
    m_frameType(
        frameType == FrameType::binary || frameType == FrameType::text
            ? frameType
            : FrameType::binary)
{
    m_socket->setRecvTimeout(0);
    m_socket->setSendTimeout(0);
    aio::AbstractAsyncChannel::bindToAioThread(m_socket->getAioThread());
    m_pingTimer->bindToAioThread(m_socket->getAioThread());
    m_aliveTimer->bindToAioThread(m_socket->getAioThread());
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
    m_pingTimer->start(pingTimeout(), [this]() { handlePingTimer(); });
    m_aliveTimer->start(m_aliveTimeout, [this]() { handleAliveTimer(); });
}

void WebSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractAsyncChannel::bindToAioThread(aioThread);
    m_socket->bindToAioThread(aioThread);
    m_pingTimer->bindToAioThread(aioThread);
    m_aliveTimer->bindToAioThread(aioThread);
}

std::chrono::milliseconds WebSocket::pingTimeout() const
{
    auto timeoutMs = (int64_t)(m_aliveTimeout.count() * kDefaultPingTimeoutMultiplier);
    return std::chrono::milliseconds(timeoutMs);
}

void WebSocket::restartTimers()
{
    m_pingTimer->cancelSync();
    m_pingTimer->start(pingTimeout(), [this]() { handlePingTimer(); });
    m_aliveTimer->cancelSync();
    m_aliveTimer->start(m_aliveTimeout, [this]() { handleAliveTimer(); });
}

void WebSocket::stopWhileInAioThread()
{
    m_pingTimer.reset();
    m_aliveTimer.reset();
    m_socket.reset();
}

void WebSocket::setIsLastFrame()
{
    dispatch([this]() { m_isLastFrame = true; });
}

void WebSocket::reportErrorIfAny(
    SystemError::ErrorCode ecode,
    size_t bytesRead,
    std::function<void(bool)> continueHandler)
{
    if (ecode != SystemError::noError)
        m_lastError = ecode;

    if (m_lastError != SystemError::noError || bytesRead == 0)
    {
        NX_DEBUG(this, lm("Reporting error %1, read queue empty: %2").args(
            m_lastError, m_readQueue.empty()));

        if (!m_readQueue.empty())
        {
            auto readData = m_readQueue.pop();
            readData.handler(m_lastError, bytesRead);
        }
        continueHandler(true);
        return;
    }

    continueHandler(false); //< No error
}

void WebSocket::handleSocketRead(SystemError::ErrorCode ecode, size_t bytesRead)
{
    reportErrorIfAny(
        ecode,
        bytesRead,
        [this, bytesRead](bool errorOccured)
        {
            if (errorOccured)
                return;

            restartTimers();
            m_parser.consume(m_readBuffer.data(), (int)bytesRead);
            m_readBuffer.resize(0);

            reportErrorIfAny(
                m_lastError,
                bytesRead,
                [this](bool errorOccured)
                {
                    if (errorOccured)
                        return;

                    processReadData();
                });
        });
}

void WebSocket::processReadData()
{
    if (m_userDataBuffer.readySize() == 0)
    {
        NX_VERBOSE(this, "User buffer is not ready. Continue reading.");
        m_socket->readSomeAsync(
            &m_readBuffer,
            [this](SystemError::ErrorCode ecode, size_t bytesRead)
            {
                handleSocketRead(ecode, bytesRead);
            });
        return;
    }

    NX_ASSERT(!m_readQueue.empty());
    auto readData = m_readQueue.pop();
    bool queueEmpty = m_readQueue.empty();
    auto handoutBuffer = m_userDataBuffer.popFront();

    NX_VERBOSE(this, lm("processReadData(): user data size: %1").arg(handoutBuffer.size()));

    readData.buffer->append(handoutBuffer);
    readData.handler(SystemError::noError, handoutBuffer.size());
    if (queueEmpty)
        return;
    readWithoutAddingToQueue();
}

void WebSocket::readWithoutAddingToQueue()
{
    post([this]() { readWithoutAddingToQueueSync(); });
}

void WebSocket::readWithoutAddingToQueueSync()
{
    if (m_userDataBuffer.readySize() != 0)
    {
        processReadData();
    }
    else
    {
        m_socket->readSomeAsync(
            &m_readBuffer,
            [this](SystemError::ErrorCode ecode, size_t bytesRead)
            {
                handleSocketRead(ecode, bytesRead);
            });
    }
}

void WebSocket::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (socketCannotRecoverFromError(m_lastError))
            {
                NX_DEBUG(this,
                    "readSomeAsync called after connection has been terminated. Ignoring.");
                handler(m_lastError, 0);
                return;
            }

            bool queueEmpty = m_readQueue.empty();
            nx::Buffer* tmp = buffer;
            m_readQueue.emplace(std::move(handler), std::move(tmp));
            NX_ASSERT(queueEmpty);
            if (queueEmpty)
                readWithoutAddingToQueueSync();
        });
}

void WebSocket::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            if (socketCannotRecoverFromError(m_lastError))
            {
                NX_DEBUG(this,
                    "sendAsync called after connection has been terminated. Ignoring.");
                handler(m_lastError, 0);
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

            sendPreparedMessage(&writeBuffer, buffer.size(), std::move(handler));
        });
}

void WebSocket::handlePingTimer()
{
    sendControlRequest(FrameType::ping);
    m_pingTimer->start(pingTimeout(), [this]() { handlePingTimer(); });
}

void WebSocket::handleAliveTimer()
{
    post([this]() { handleSocketRead(SystemError::timedOut, 0); });
}

void WebSocket::setAliveTimeout(std::chrono::milliseconds timeout)
{
    m_aliveTimeout = timeout;
}

void WebSocket::sendCloseAsync()
{
    post(
        [this]()
        {
            sendControlRequest(FrameType::close);
            reportErrorIfAny(SystemError::connectionAbort, 0, [](bool) {});
        });
}

void WebSocket::handleSocketWrite(SystemError::ErrorCode ecode, size_t /*bytesSent*/)
{
    if (m_writeQueue.empty())
    {
        NX_VERBOSE(this, "write queue is empty");
        return; // might be the case on object destruction
    }

    auto writeData = m_writeQueue.pop();
    bool queueEmpty = m_writeQueue.empty();
    {
        nx::utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
        if (writeData.handler)
            writeData.handler(ecode, writeData.buffer.writeSize);
        if (watcher.objectDestroyed())
            return;
    }
    if (!queueEmpty)
        m_socket->sendAsync(
            m_writeQueue.first().buffer.buffer,
            [this](SystemError::ErrorCode ecode, size_t bytesSent)
            {
                handleSocketWrite(ecode, bytesSent);
            });
}

void WebSocket::sendPreparedMessage(nx::Buffer* buffer, int writeSize, IoCompletionHandler handler)
{
    WriteData writeData(std::move(*buffer), writeSize);
    bool queueEmpty = m_writeQueue.empty();
    m_writeQueue.emplace(std::move(handler), std::move(writeData));
    if (queueEmpty)
        m_socket->sendAsync(
            m_writeQueue.first().buffer.buffer,
            [this](SystemError::ErrorCode ecode, size_t bytesSent)
            {
                handleSocketWrite(ecode, bytesSent);
            });
}

void WebSocket::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    nx::utils::promise<void> p;
    auto f = p.get_future();

    dispatch(
        [this, eventType, p = std::move(p)] ()
        {
            m_pingTimer->cancelSync();
            m_aliveTimer->cancelSync();
            m_socket->cancelIOSync(eventType);

        });

    f.wait();
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

    m_userDataBuffer.append(data, len);

    if (m_receiveMode == ReceiveMode::stream)
        m_userDataBuffer.lock();
}

void WebSocket::frameEnded()
{
    NX_VERBOSE(this, lm("Frame ended. Type: %1, size from header: %2").args(
        m_parser.frameType(),
        m_parser.frameSize()));

    if (m_parser.frameType() == FrameType::ping)
    {
        NX_VERBOSE(this, "Ping received.");
        sendControlResponse(FrameType::ping, FrameType::pong);
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
        if (m_lastError == SystemError::noError)
        {
            NX_DEBUG(this, "Received close request, responding and terminating.");
            sendControlResponse(FrameType::close, FrameType::close);
            m_lastError = SystemError::connectionAbort;
            return;
        }
        else
        {
            NX_DEBUG(this, "Received close response, terminating.");
            NX_ASSERT(m_controlBuffer.isEmpty());
        }

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

    m_userDataBuffer.lock();
}

void WebSocket::sendControlResponse(FrameType requestType, FrameType responseType)
{
    nx::Buffer responseFrame;
    m_serializer.prepareMessage(m_controlBuffer, responseType, &responseFrame);
    auto writeSize = m_controlBuffer.size();
    m_controlBuffer.resize(0);

    sendPreparedMessage(
        &responseFrame,
        writeSize,
        [this, requestType](SystemError::ErrorCode ecode, size_t)
        {
            reportErrorIfAny(
                ecode,
                1, //< just not zero
                [this, requestType](bool errorOccured)
                {
                    if (errorOccured)
                    {
                        NX_VERBOSE(this, lm("Control response for %1 failed.").arg(
                            frameTypeString(requestType)));
                        return;
                    }

                    NX_VERBOSE(this, lm("Control response for %1 successfully sent.").arg(
                        frameTypeString(requestType)));
                });
        });
}

void WebSocket::sendControlRequest(FrameType type)
{
    nx::Buffer requestFrame;
    m_serializer.prepareMessage("", type, &requestFrame);

    sendPreparedMessage(
        &requestFrame,
        0,
        [this, type](SystemError::ErrorCode ecode, size_t)
        {
            reportErrorIfAny(
                ecode,
                1, //< just not zero
                [this, type](bool errorOccured)
                {
                    if (errorOccured)
                    {
                        NX_VERBOSE(this, lm("Control request for %1 failed.").arg(
                            frameTypeString(type)));
                        return;
                    }

                    NX_VERBOSE(this, lm("Control request for %1 successfully sent.").arg(
                        frameTypeString(type)));
                });
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
    m_userDataBuffer.lock();
}

void WebSocket::handleError(Error err)
{
    NX_DEBUG(this, lm("Parse error %1. Closing connection.").arg((int) err));
    sendControlRequest(FrameType::close);
    m_lastError = SystemError::connectionAbort;
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
