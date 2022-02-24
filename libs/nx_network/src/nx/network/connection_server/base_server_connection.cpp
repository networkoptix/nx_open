// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_server_connection.h"

namespace nx::network::server {

BaseServerConnection::BaseServerConnection(
    std::unique_ptr<AbstractStreamSocket> streamSocket)
    :
    m_streamSocket(std::move(streamSocket))
{
    bindToAioThread(m_streamSocket->getAioThread());

    m_readBuffer.reserve(kReadBufferCapacity);
}

void BaseServerConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_streamSocket)
        m_streamSocket->bindToAioThread(aioThread);
}

void BaseServerConnection::startReadingConnection(
    std::optional<std::chrono::milliseconds> inactivityTimeout)
{
    dispatch(
        [this, inactivityTimeout]()
        {
            setInactivityTimeout(inactivityTimeout);
            if (!m_streamSocket->setNonBlockingMode(true))
            {
                const auto errorCode = SystemError::getLastOSErrorCode();

                NX_VERBOSE(this, "Connection %1-%2 configuration error. %3",
                    m_streamSocket->getLocalAddress(), m_streamSocket->getForeignAddress(),
                    SystemError::toString(errorCode));

                return m_streamSocket->post(
                    [this, errorCode]() { onBytesRead(errorCode, (size_t) -1); });
            }

            m_readingConnection = true;

            m_streamSocket->readSomeAsync(
                &m_readBuffer,
                [this](auto&&... args) { onBytesRead(std::move(args)...); });
        });
}

void BaseServerConnection::stopReadingConnection()
{
    dispatch(
        [this]()
        {
            m_readingConnection = false;
            m_streamSocket->cancelIOSync(aio::EventType::etRead);
            removeInactivityTimer();
        });
}

bool BaseServerConnection::isReadingConnection() const
{
    return m_readingConnection;
}

void BaseServerConnection::sendBufAsync(const nx::Buffer* buf)
{
    NX_ASSERT(m_streamSocket);

    dispatch(
        [this, buf]()
        {
            m_isSendingData = true;
            if (m_inactivityTimeout)
                removeInactivityTimer();

            if (m_streamSocket)
            {
                m_streamSocket->sendAsync(
                    buf,
                    [this](auto&&... args) { onBytesSent(std::move(args)...); });
            }
            else
            {
                post([this]() { onBytesSent(SystemError::notConnected, (std::size_t) -1); });
            }
            m_bytesToSend = buf->size();
        });
}

void BaseServerConnection::cancelRead()
{
    executeInAioThreadSync([this]() { stopReadingConnection(); });
}

void BaseServerConnection::cancelWrite()
{
    executeInAioThreadSync(
        [this]()
        {
            m_isSendingData = false;
            if (m_streamSocket)
                m_streamSocket->cancelIOSync(aio::etWrite);
            m_bytesToSend = 0;
        });
}

void BaseServerConnection::closeConnection(SystemError::ErrorCode closeReason)
{
    dispatch(
        [this, closeReason]()
        {
            nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
            triggerConnectionClosedEvent(closeReason);
            if (watcher.interrupted())
                return;

            m_streamSocket.reset();
        });
}

int BaseServerConnection::registerCloseHandler(OnConnectionClosedHandler handler)
{
    auto id = ++m_lastConnectionClosedHandlerId;
    m_connectionClosedHandlers.emplace(id, std::move(handler));
    return id;
}

void BaseServerConnection::removeCloseHandler(int id)
{
    m_connectionClosedHandlers.erase(id);
}

bool BaseServerConnection::isSsl() const
{
    if (auto s = dynamic_cast<AbstractEncryptedStreamSocket*>(m_streamSocket.get()))
        return s->isEncryptionEnabled();

    return false;
}

const std::unique_ptr<AbstractStreamSocket>& BaseServerConnection::socket() const
{
    return m_streamSocket;
}

std::unique_ptr<AbstractStreamSocket> BaseServerConnection::takeSocket()
{
    m_streamSocket->cancelIOSync(aio::etNone);

    return std::exchange(m_streamSocket, nullptr);
}

void BaseServerConnection::setInactivityTimeout(std::optional<std::chrono::milliseconds> value)
{
    NX_ASSERT(m_streamSocket->isInSelfAioThread());

    // NOTE: Here, std::nullopt means "no timeout". But, on the API level,
    // "std::nullopt" means "default timeout" and kNoTimeout is used for "no timeout".
    if (value && value == kNoTimeout)
        value = std::nullopt;

    m_inactivityTimeout = value;

    if (value)
        resetInactivityTimer();
    else
        removeInactivityTimer();
}

std::optional<std::chrono::milliseconds> BaseServerConnection::inactivityTimeout() const
{
    return m_inactivityTimeout;
}

std::size_t BaseServerConnection::totalBytesReceived() const
{
    return m_totalBytesReceived;
}

void BaseServerConnection::stopWhileInAioThread()
{
    nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
    triggerConnectionClosedEvent(SystemError::noError);
    if (watcher.interrupted())
        return;
    m_streamSocket.reset();
}

SocketAddress BaseServerConnection::getForeignAddress() const
{
    return m_streamSocket ? m_streamSocket->getForeignAddress() : SocketAddress();
}

void BaseServerConnection::onBytesRead(SystemError::ErrorCode errorCode, size_t bytesRead)
{
    resetInactivityTimer();
    if (errorCode != SystemError::noError)
        return handleSocketError(errorCode);

    m_totalBytesReceived += bytesRead;

    NX_ASSERT(
        (size_t)m_readBuffer.size() >= bytesRead,
        nx::format("%1 vs %2").args(m_readBuffer.size(), bytesRead));

    {
        nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
        bytesReceived(m_readBuffer);
        if (watcher.interrupted())
            return; //< Connection has been removed by handler.
    }

    m_readBuffer.resize(0);

    if (!m_streamSocket)
        return;

    if (bytesRead == 0)    //< Connection closed by remote peer.
    {
        NX_VERBOSE(this, "Connection %1-%2 is closed by remote peer",
            m_streamSocket->getLocalAddress(), m_streamSocket->getForeignAddress());
        return handleSocketError(SystemError::connectionReset);
    }

    if (!m_readingConnection)
        return;

    m_streamSocket->readSomeAsync(
        &m_readBuffer,
        [this](auto&&... args) { onBytesRead(std::move(args)...); });
}

void BaseServerConnection::onBytesSent(
    SystemError::ErrorCode errorCode,
    [[maybe_unused]] size_t count)
{
    m_isSendingData = false;
    resetInactivityTimer();

    if (errorCode != SystemError::noError)
        return handleSocketError(errorCode);

    NX_ASSERT(count == m_bytesToSend);

    readyToSendData();
}

void BaseServerConnection::handleSocketError(SystemError::ErrorCode errorCode)
{
    NX_ASSERT(errorCode != SystemError::noError);
    closeConnection(errorCode);
}

void BaseServerConnection::triggerConnectionClosedEvent(SystemError::ErrorCode closeReason)
{
    auto connectionClosedHandlers = std::exchange(m_connectionClosedHandlers, {});
    nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
    for (auto& [id, connectionCloseHandler]: connectionClosedHandlers)
        connectionCloseHandler(closeReason, watcher.interrupted());
}

void BaseServerConnection::resetInactivityTimer()
{
    if (!m_inactivityTimeout || m_isSendingData)
        return;

    m_streamSocket->registerTimer(
        *m_inactivityTimeout,
        [this]()
        {
            NX_VERBOSE(this, "Closing connection from %1 to %2 by inactivity timeout",
                m_streamSocket->getForeignAddress(), m_streamSocket->getLocalAddress());
            handleSocketError(SystemError::timedOut);
        });
}

void BaseServerConnection::removeInactivityTimer()
{
    m_streamSocket->cancelIOSync(aio::etTimedOut);
}

//-------------------------------------------------------------------------------------------------

BaseServerConnectionWrapper::BaseServerConnectionWrapper(
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    BaseServerConnectionHandler* handler)
    :
    BaseServerConnection(std::move(streamSocket)),
    m_handler(handler)
{
}

void BaseServerConnectionWrapper::bytesReceived(const nx::Buffer& buf)
{
    m_handler->bytesReceived(buf);
}

void BaseServerConnectionWrapper::readyToSendData()
{
    m_handler->readyToSendData();
}

} // namespace nx::network::server
