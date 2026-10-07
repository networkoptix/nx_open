// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/pollable.h>
#include <nx/network/async_stoppable.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>

#include "stream_socket_server.h"

namespace nx::network::server {

static constexpr size_t kReadBufferCapacity = 16 * 1024;

using OnConnectionClosedHandler =
    nx::MoveOnlyFunc<void(SystemError::ErrorCode /*closeReason*/, bool /*connectionDestroyed*/)>;

namespace detail { using CloseHandlers = std::map<int /*id*/, OnConnectionClosedHandler>; }

class CloseHandlerRegistry;

/**
 * Keeps a close handler registered for as long as it lives. A registrar that does not own the
 * connection MUST hold the subscription, otherwise its handler may be invoked after it is gone.
 * Outliving the connection is safe. Canceling, by reset(), assignment or destruction, edits the
 * connection's handler list unlocked, so it MUST run in the connection's AIO thread or after the
 * connection is stopped.
 */
class CloseHandlerSubscription
{
public:
    CloseHandlerSubscription() = default;
    CloseHandlerSubscription(CloseHandlerSubscription&&) = default;
    ~CloseHandlerSubscription() { reset(); }

    CloseHandlerSubscription& operator=(CloseHandlerSubscription&& other)
    {
        if (this != &other)
        {
            reset();
            m_handlers = std::move(other.m_handlers);
            m_id = std::exchange(other.m_id, 0);
        }
        return *this;
    }

    /** Cancels the handler. Takes effect even while the close handlers are being invoked. */
    void reset()
    {
        // The handlers outlive the connection while they are being invoked, which is exactly
        // when a cancellation still has to be honored.
        if (const auto handlers = m_handlers.lock())
            handlers->erase(m_id);
        m_handlers.reset();
        m_id = 0;
    }

    /** Keeps the handler registered. Only for a registrar that outlives the connection. */
    void release()
    {
        m_handlers.reset();
        m_id = 0;
    }

private:
    friend class CloseHandlerRegistry;

    CloseHandlerSubscription(std::weak_ptr<detail::CloseHandlers> handlers, int id):
        m_handlers(std::move(handlers)),
        m_id(id)
    {
    }

    std::weak_ptr<detail::CloseHandlers> m_handlers;
    int m_id = 0;
};

/**
 * The close handlers of a single connection.
 */
class CloseHandlerRegistry
{
public:
    [[nodiscard]] CloseHandlerSubscription add(OnConnectionClosedHandler handler)
    {
        const int id = ++m_lastId;
        m_handlers->emplace(id, std::move(handler));
        return CloseHandlerSubscription(m_handlers, id);
    }

    /**
     * Invokes the handlers in the registration order, extracting each one before invoking it, so
     * a handler is free to destroy the connection or to cancel a handler that follows it.
     * Handlers registered while this runs are left to the next call.
     * @param connectionFreedFlag Interrupted by the connection destruction. Its state is passed
     *     to the handlers as connectionDestroyed.
     */
    void invokeAll(
        SystemError::ErrorCode closeReason, nx::utils::InterruptionFlag* connectionFreedFlag)
    {
        // The local copy keeps the handlers alive if one of them destroys the connection.
        const auto handlers = m_handlers;
        if (handlers->empty())
            return;

        const int lastId = handlers->rbegin()->first;

        nx::utils::InterruptionFlag::Watcher watcher(connectionFreedFlag);
        while (!handlers->empty() && handlers->begin()->first <= lastId)
        {
            auto node = handlers->extract(handlers->begin());
            node.mapped()(closeReason, watcher.interrupted());
        }
    }

private:
    std::shared_ptr<detail::CloseHandlers> m_handlers = std::make_shared<detail::CloseHandlers>();
    int m_lastId = 0;
};

/**
 * Contains common logic for server-side connection created by StreamSocketServer.
 *
 * NOTE: This class is not thread-safe. All methods are expected to be executed in the aio thread
 * the underlying socket is bound to. In other case, it is a caller's responsibility to synchronize
 * the access to the connection object.
 * NOTE: Despite absence of thread-safety simultaneous read/write operations are allowed in
 * different threads
 * NOTE: This class instance can be safely freed in any event handler (i.e., in internal socket's
 * aio thread)
 * NOTE: It is allowed to free instance within event handler
 */
class NX_NETWORK_API BaseServerConnection:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using OnConnectionClosedHandler = server::OnConnectionClosedHandler;

    BaseServerConnection(
        std::unique_ptr<AbstractStreamSocket> streamSocket);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Start receiving data from connection.
     */
    void startReadingConnection(
        std::optional<std::chrono::milliseconds> inactivityTimeout = std::nullopt);

    void stopReadingConnection();

    /**
     * @param buf Must be valid until send completion.
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
     * WARNING: Handler may be invoked after the connection object is destroyed, so the returned
     * subscription MUST be kept for as long as the handler may use the registrar.
     */
    [[nodiscard]] CloseHandlerSubscription registerCloseHandler(OnConnectionClosedHandler handler)
    {
        return m_closeHandlers.add(std::move(handler));
    }

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
    CloseHandlerRegistry m_closeHandlers;
    nx::utils::InterruptionFlag m_connectionFreedFlag;
    std::size_t m_totalBytesReceived = 0;

    std::optional<std::chrono::milliseconds> m_inactivityTimeout;
    bool m_isSendingData = false;
    bool m_readingConnection = false;

    void onBytesRead(SystemError::ErrorCode errorCode, size_t bytesRead);
    void onBytesSent(SystemError::ErrorCode errorCode, size_t count);
    void handleSocketError(SystemError::ErrorCode errorCode);
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
