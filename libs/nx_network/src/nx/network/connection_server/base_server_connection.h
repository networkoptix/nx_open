#pragma once

#include <forward_list>
#include <functional>
#include <memory>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/pollable.h>
#include <nx/network/async_stoppable.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/std/optional.h>

#include "stream_socket_server.h"

namespace nx::network::server {

static constexpr size_t kReadBufferCapacity = 16 * 1024;

struct BaseServerConnectionAccess
{
    template<typename Derived, typename Base>
    static void bytesReceived(Base* base, nx::Buffer& buffer)
    {
        static_cast<Derived*>(base)->bytesReceived(buffer);
    }

    template<typename Derived, typename Base>
    static void readyToSendData(Base* base)
    {
        static_cast<Derived*>(base)->readyToSendData();
    }
};

/**
 * Contains common logic for server-side connection created by StreamSocketServer.
 * CustomConnectionType MUST implement following methods:
 * @code {*.cpp}
 *     //Received data from remote host. Empty buffer signals end-of-file.
 *     void bytesReceived( const nx::Buffer& buf );
 *     //Submitted data has been sent, m_writeBuffer has some free space.
 *     void readyToSendData();
 * @endcode
 *
 * CustomConnectionManagerType MUST implement following types:
 * @code {*.cpp}
 *     //Type of connections, handled by server. BaseServerConnection<CustomConnectionType, CustomConnectionManagerType>
 *     //MUST be convertible to ConnectionType with static_cast
 *     typedef ... ConnectionType;
 * @endcode
 * and method(s):
 * @code {*.cpp}
 *     //This connection is passed here just after socket has been terminated.
 *     closeConnection( SystemError::ErrorCode reasonCode, CustomConnectionType* )
 * @endcode
 *
 * NOTE: This class is not thread-safe. All methods are expected to be executed in aio thread, undelying socket is bound to.
 *     In other case, it is caller's responsibility to syunchronize access to the connection object.
 * NOTE: Despite absence of thread-safety simultaneous read/write operations are allowed in different threads
 * NOTE: This class instance can be safely freed in any event handler (i.e., in internal socket's aio thread)
 * NOTE: It is allowed to free instance within event handler
 */
template<
    class CustomConnectionType
> class BaseServerConnection:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;
    using self_type = BaseServerConnection<CustomConnectionType>;

public:
    using OnConnectionClosedHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode /*closeReason*/,
        CustomConnectionType* /*connection*/)>;

    /**
     * @param connectionManager When connection is finished,
     * connectionManager->closeConnection(closeReason, this) is called.
     */
    BaseServerConnection(
        OnConnectionClosedHandler onConnectionClosedHandler,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        m_onConnectionClosedHandler(std::move(onConnectionClosedHandler)),
        m_streamSocket(std::move(streamSocket))
    {
        bindToAioThread(m_streamSocket->getAioThread());

        m_readBuffer.reserve(kReadBufferCapacity);
    }

    ~BaseServerConnection()
    {
        stopWhileInAioThread();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        if (m_streamSocket)
            m_streamSocket->bindToAioThread(aioThread);
    }

    /**
     * Start receiving data from connection.
     */
    void startReadingConnection(
        std::optional<std::chrono::milliseconds> inactivityTimeout = std::nullopt)
    {
        m_streamSocket->dispatch(
            [this, inactivityTimeout]()
            {
                setInactivityTimeout(inactivityTimeout);
                if (!m_streamSocket->setNonBlockingMode(true))
                {
                    return m_streamSocket->post(
                        [this, code = SystemError::getLastOSErrorCode()]()
                        {
                            onBytesRead(code, (size_t) -1);
                        });
                }

                m_receiving = true;
                m_readBuffer.resize(0);
                m_streamSocket->readSomeAsync(
                    &m_readBuffer,
                    [this](auto&&... args) { onBytesRead(std::move(args)...); });
            });
    }

    void stopReading()
    {
        NX_ASSERT(isInSelfAioThread());
        m_receiving = false;
    }

    void setReceivingStarted()
    {
        NX_ASSERT(isInSelfAioThread());
        m_receiving = true;
    }

    /**
     * @return true, if started async send operation.
     */
    void sendBufAsync(const nx::Buffer& buf)
    {
        m_streamSocket->dispatch(
            [this, &buf]()
            {
                m_isSendingData = true;
                if (m_inactivityTimeout)
                    removeInactivityTimer();

                m_streamSocket->sendAsync(
                    buf,
                    [this](auto&&... args) { onBytesSent(std::move(args)...); });
                m_bytesToSend = buf.size();
            });
    }

    void closeConnection(SystemError::ErrorCode closeReasonCode)
    {
        if (m_onConnectionClosedHandler)
        {
            nx::utils::swapAndCall(
                m_onConnectionClosedHandler,
                closeReasonCode,
                static_cast<CustomConnectionType*>(this));
        }
    }

    /**
     * Register handler to be executed when connection just about to be destroyed.
     * NOTE: Handler is invoked in socket's aio thread.
     */
    void registerCloseHandler(nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_connectionClosedHandlers.push_front(std::move(handler));
    }

    bool isSsl() const
    {
        if (auto s = dynamic_cast<AbstractEncryptedStreamSocket*>(m_streamSocket.get()))
            return s->isEncryptionEnabled();

        return false;
    }

    const std::unique_ptr<AbstractStreamSocket>& socket() const
    {
        return m_streamSocket;
    }

    /**
     * Moves socket to the caller.
     * BaseServerConnection instance MUST be deleted just after this call.
     */
    virtual std::unique_ptr<AbstractStreamSocket> takeSocket()
    {
        m_streamSocket->cancelIOSync(aio::etNone);
        m_receiving = false;

        return std::exchange(m_streamSocket, nullptr);
    }

    /**
     * NOTE: Can be called only from connection's AIO thread.
     */
    void setInactivityTimeout(std::optional<std::chrono::milliseconds> value)
    {
        NX_ASSERT(m_streamSocket->pollable()->isInSelfAioThread());
        m_inactivityTimeout = value;

        if (value)
            resetInactivityTimer();
        else
            removeInactivityTimer();
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        m_streamSocket.reset();
        triggerConnectionClosedEvent();
    }

    SocketAddress getForeignAddress() const
    {
        return m_streamSocket->getForeignAddress();
    }

private:
    OnConnectionClosedHandler m_onConnectionClosedHandler;
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    nx::Buffer m_readBuffer;
    size_t m_bytesToSend = 0;
    std::forward_list<nx::utils::MoveOnlyFunc<void()>> m_connectionClosedHandlers;
    nx::utils::ObjectDestructionFlag m_connectionFreedFlag;

    std::optional<std::chrono::milliseconds> m_inactivityTimeout;
    bool m_isSendingData = false;
    bool m_receiving = false;

    void onBytesRead(SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        resetInactivityTimer();
        if (errorCode != SystemError::noError)
            return handleSocketError(errorCode);

        NX_ASSERT((size_t)m_readBuffer.size() == bytesRead);

        {
            nx::utils::ObjectDestructionFlag::Watcher watcher(&m_connectionFreedFlag);
            BaseServerConnectionAccess::bytesReceived<CustomConnectionType>(this, m_readBuffer);
            if (watcher.objectDestroyed())
                return; //< Connection has been removed by handler.
        }

        m_readBuffer.resize(0);

        if (!m_receiving)
            return;

        if (bytesRead == 0)    //< Connection closed by remote peer.
            return handleSocketError(SystemError::connectionReset);

        m_streamSocket->readSomeAsync(
            &m_readBuffer,
            [this](auto&&... args) { onBytesRead(std::move(args)...); });
    }

    void onBytesSent(
        SystemError::ErrorCode errorCode,
        [[maybe_unused]] size_t count)
    {
        m_isSendingData = false;
        resetInactivityTimer();

        if (errorCode != SystemError::noError)
            return handleSocketError(errorCode);

        NX_ASSERT(count == m_bytesToSend);

        BaseServerConnectionAccess::readyToSendData<CustomConnectionType>(this);
    }

    void handleSocketError(SystemError::ErrorCode errorCode)
    {
        nx::utils::ObjectDestructionFlag::Watcher watcher(&m_connectionFreedFlag);
        switch (errorCode)
        {
            case SystemError::noError:
                NX_ASSERT(false);
                break;

            default:
                closeConnection(errorCode);
                break;
        }

        if (!watcher.objectDestroyed())
            triggerConnectionClosedEvent();
    }

    void triggerConnectionClosedEvent()
    {
        auto connectionClosedHandlers = std::exchange(m_connectionClosedHandlers, {});
        for (auto& connectionCloseHandler: connectionClosedHandlers)
            connectionCloseHandler();
    }

    void resetInactivityTimer()
    {
        if (!m_inactivityTimeout || m_isSendingData)
            return;

        m_streamSocket->registerTimer(
            *m_inactivityTimeout,
            [this]() { handleSocketError(SystemError::timedOut); });
    }

    void removeInactivityTimer()
    {
        m_streamSocket->cancelIOSync(aio::etTimedOut);
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * These two classes enable BaseServerConnection alternative usage without inheritance.
 */

class BaseServerConnectionHandler
{
public:
    virtual ~BaseServerConnectionHandler() = default;

    virtual void bytesReceived(const nx::Buffer& buffer) = 0;
    virtual void readyToSendData() = 0;
};

class BaseServerConnectionWrapper:
    public BaseServerConnection<BaseServerConnectionWrapper>
{
    friend struct BaseServerConnectionAccess;

public:
    BaseServerConnectionWrapper(
        OnConnectionClosedHandler onConnectionClosedHandler,
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        BaseServerConnectionHandler* handler)
        :
        BaseServerConnection<BaseServerConnectionWrapper>(
            std::move(onConnectionClosedHandler),
            std::move(streamSocket)),
        m_handler(handler)
    {
    }

private:
    void bytesReceived(const nx::Buffer& buf)
    {
        m_handler->bytesReceived(buf);
    }

    void readyToSendData()
    {
        m_handler->readyToSendData();
    }

private:
    BaseServerConnectionHandler* m_handler = nullptr;
};

} // namespace nx::network::server
