// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <map>
#include <memory>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/pollable.h>
#include <nx/network/async_stoppable.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/std/optional.h>

#include "stream_socket_server.h"

namespace nx::network::server {

static constexpr size_t kReadBufferCapacity = 16 * 1024;

/**
 * Contains common logic for server-side connection created by StreamSocketServer.
 *
 * NOTE: This class is not thread-safe. All methods are expected to be executed in aio thread, undelying socket is bound to.
 *     In other case, it is caller's responsibility to syunchronize access to the connection object.
 * NOTE: Despite absence of thread-safety simultaneous read/write operations are allowed in different threads
 * NOTE: This class instance can be safely freed in any event handler (i.e., in internal socket's aio thread)
 * NOTE: It is allowed to free instance within event handler
 */
class NX_NETWORK_API BaseServerConnection:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using OnConnectionClosedHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode /*closeReason*/)>;

    BaseServerConnection(
        std::unique_ptr<AbstractStreamSocket> streamSocket);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Start receiving data from connection.
     */
    void startReadingConnection(
        std::optional<std::chrono::milliseconds> inactivityTimeout = std::nullopt);

    void stopReadingConnection();

    bool isReadingConnection() const;

    /**
     * @param buf Must be valid untilsend completion.
     */
    void sendBufAsync(const nx::Buffer* buf);

    /**
     * See AbstractAsyncChannel::cancelRead.
     */
    virtual void cancelRead();

    /**
     * See AbstractAsyncChannel::cancelWrite.
     */
    virtual void cancelWrite();

    void closeConnection(SystemError::ErrorCode closeReason);

    /**
     * Register handler to be executed when connection just about to be destroyed.
     * NOTE: Handler is invoked in socket's aio thread.
     * @return Id that may be used to remove the handler.
     */
    int registerCloseHandler(OnConnectionClosedHandler handler);

    /**
     * @param id returned by BaseServerConnection::registerCloseHandler.
     */
    void removeCloseHandler(int id);

    bool isSsl() const;

    const std::unique_ptr<AbstractStreamSocket>& socket() const;

    /**
     * Moves socket to the caller.
     * BaseServerConnection instance MUST be deleted just after this call.
     */
    virtual std::unique_ptr<AbstractStreamSocket> takeSocket();

    /**
     * NOTE: Can be called only from connection's AIO thread.
     */
    void setInactivityTimeout(std::optional<std::chrono::milliseconds> value);

    std::optional<std::chrono::milliseconds> inactivityTimeout() const;

    std::size_t totalBytesReceived() const;

protected:
    virtual void bytesReceived(const nx::Buffer& buffer) = 0;
    virtual void readyToSendData() = 0;

    virtual void stopWhileInAioThread() override;

    SocketAddress getForeignAddress() const;

private:
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    nx::Buffer m_readBuffer;
    size_t m_bytesToSend = 0;
    std::map<int /*id*/, OnConnectionClosedHandler> m_connectionClosedHandlers;
    std::atomic<int> m_lastConnectionClosedHandlerId = 0;
    nx::utils::InterruptionFlag m_connectionFreedFlag;
    std::size_t m_totalBytesReceived = 0;

    std::optional<std::chrono::milliseconds> m_inactivityTimeout;
    bool m_isSendingData = false;
    bool m_readingConnection = false;

    void onBytesRead(SystemError::ErrorCode errorCode, size_t bytesRead);
    void onBytesSent(SystemError::ErrorCode errorCode, size_t count);
    void handleSocketError(SystemError::ErrorCode errorCode);
    void triggerConnectionClosedEvent(SystemError::ErrorCode closeReason);
    void resetInactivityTimer();
    void removeInactivityTimer();
};

//-------------------------------------------------------------------------------------------------

/**
 * These two classes enable BaseServerConnection alternative usage without inheritance.
 */

class NX_NETWORK_API BaseServerConnectionHandler
{
public:
    virtual ~BaseServerConnectionHandler() = default;

    virtual void bytesReceived(const nx::Buffer& buffer) = 0;
    virtual void readyToSendData() = 0;
};

class NX_NETWORK_API BaseServerConnectionWrapper:
    public BaseServerConnection
{
public:
    BaseServerConnectionWrapper(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        BaseServerConnectionHandler* handler);

private:
    virtual void bytesReceived(const nx::Buffer& buf) override;
    virtual void readyToSendData() override;

private:
    BaseServerConnectionHandler* m_handler = nullptr;
};

} // namespace nx::network::server
