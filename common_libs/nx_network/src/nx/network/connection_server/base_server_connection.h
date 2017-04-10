#pragma once

#include <forward_list>
#include <functional>
#include <memory>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/pollable.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/object_destruction_flag.h>
#include <utils/common/stoppable.h>

#include "stream_socket_server.h"

namespace nx_api {

static constexpr size_t READ_BUFFER_CAPACITY = 16 * 1024;

struct BaseServerConnectionAccess
{
    template<typename Derived, typename Base>
    static void bytesReceived(Base* base, nx::Buffer& buffer)
    {
        static_cast<Derived*>(base)->on_bytesReceived(buffer);
    }

    template<typename Derived, typename Base>
    static void readyToSendData(Base* base)
    {
        static_cast<Derived*>(base)->on_readyToSendData();
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
 * @note This class is not thread-safe. All methods are expected to be executed in aio thread, undelying socket is bound to.
 *     In other case, it is caller's responsibility to syunchronize access to the connection object.
 * @note Despite absence of thread-safety simultaneous read/write operations are allowed in different threads
 * @note This class instance can be safely freed in any event handler (i.e., in internal socket's aio thread)
 * @note It is allowed to free instance within event handler
 */
template<
    class CustomConnectionType
> class BaseServerConnection:
    public nx::network::aio::BasicPollable
{
    typedef nx::network::aio::BasicPollable BaseType;

public:
    typedef BaseServerConnection<CustomConnectionType> SelfType;

    /**
     * @param connectionManager When connection is finished,
     * connectionManager->closeConnection(closeReason, this) is called.
     */
    BaseServerConnection(
        StreamConnectionHolder<CustomConnectionType>* connectionManager,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        m_connectionManager(connectionManager),
        m_streamSocket(std::move(streamSocket)),
        m_bytesToSend(0),
        m_isSendingData(false)
    {
        bindToAioThread(m_streamSocket->getAioThread());

        m_readBuffer.reserve(READ_BUFFER_CAPACITY);
    }

    ~BaseServerConnection()
    {
        stopWhileInAioThread();
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        BaseType::bindToAioThread(aioThread);

        m_streamSocket->bindToAioThread(aioThread);
    }

    /**
     * Start receiving data from connection
     * @return false, if could not start asynchronous operation
     * (this can happen due to lack of resources on host machine).
     */
    void startReadingConnection(
        boost::optional<std::chrono::milliseconds> inactivityTimeout = boost::none)
    {
        using namespace std::placeholders;

        m_streamSocket->dispatch(
            [this, inactivityTimeout]()
            {
                m_receiving = true;
                setInactivityTimeout(inactivityTimeout);
                if (!m_streamSocket->setNonBlockingMode(true))
                    return onBytesRead(SystemError::getLastOSErrorCode(), (size_t)-1);

                m_streamSocket->readSomeAsync(
                    &m_readBuffer,
                    std::bind(&SelfType::onBytesRead, this, _1, _2));
            });
    }

    void stopReading()
    {
        NX_ASSERT(isInSelfAioThread());
        m_receiving = false;
    }

    /**
     * @return true, if started async send operation.
     */
    void sendBufAsync(const nx::Buffer& buf)
    {
        using namespace std::placeholders;

        m_streamSocket->dispatch(
            [this, &buf]()
            {
                m_isSendingData = true;
                if (m_inactivityTimeout)
                    removeInactivityTimer();

                m_streamSocket->sendAsync(
                    buf,
                    std::bind(&SelfType::onBytesSent, this, _1, _2));
                m_bytesToSend = buf.size();

            });
    }

    void closeConnection(SystemError::ErrorCode closeReasonCode)
    {
        m_connectionManager->closeConnection(
            closeReasonCode,
            static_cast<CustomConnectionType*>(this));
    }

    /**
     * Register handler to be executed when connection just about to be destroyed.
     * @note It is unspecified which thread handler will be invoked in
     * (usually, it is aio thread corresponding to the socket).
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
    std::unique_ptr<AbstractStreamSocket> takeSocket()
    {
        auto socket = std::move(m_streamSocket);
        socket->cancelIOSync(nx::network::aio::etNone);
        m_streamSocket = nullptr;
        return socket;
    }

    /**
     * @note Can be called inly from connection's AIO thread.
     */
    void setInactivityTimeout(boost::optional<std::chrono::milliseconds> value)
    {
        NX_EXPECT(m_streamSocket->pollable()->isInSelfAioThread());
        m_inactivityTimeout = value;

        if (value)
            resetInactivityTimer();
        else
            removeInactivityTimer();
    }

protected:
    SocketAddress getForeignAddress() const
    {
        return m_streamSocket->getForeignAddress();
    }

    StreamConnectionHolder<CustomConnectionType>* connectionManager()
    {
        return m_connectionManager;
    }

private:
    StreamConnectionHolder<CustomConnectionType>* m_connectionManager;
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    nx::Buffer m_readBuffer;
    size_t m_bytesToSend;
    std::forward_list<nx::utils::MoveOnlyFunc<void()>> m_connectionClosedHandlers;
    nx::utils::ObjectDestructionFlag m_connectionFreedFlag;

    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;
    bool m_isSendingData;
    bool m_receiving = false;

    virtual void stopWhileInAioThread() override
    {
        m_streamSocket.reset();
        triggerConnectionClosedEvent();
    }

    void onBytesRead(SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        using namespace std::placeholders;

        resetInactivityTimer();
        if (errorCode != SystemError::noError)
            return handleSocketError(errorCode);

        NX_ASSERT((size_t)m_readBuffer.size() == bytesRead);

        {
            nx::utils::ObjectDestructionFlag::Watcher watcher(&m_connectionFreedFlag);
            BaseServerConnectionAccess::bytesReceived<CustomConnectionType>(this, m_readBuffer);
            if (watcher.objectDestroyed() || !m_receiving)
                return; //< Connection has been removed by handler.
        }

        if (bytesRead == 0)    //< Connection closed by remote peer.
            return handleSocketError(SystemError::connectionReset);

        m_readBuffer.resize(0);
        m_streamSocket->readSomeAsync(
            &m_readBuffer,
            std::bind(&SelfType::onBytesRead, this, _1, _2));
    }

    void onBytesSent(SystemError::ErrorCode errorCode, size_t count)
    {
        m_isSendingData = false;
        resetInactivityTimer();

        if (errorCode != SystemError::noError)
            return handleSocketError(errorCode);

        static_cast<void>(count);
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
                m_connectionManager->closeConnection(
                    errorCode,
                    static_cast<CustomConnectionType*>(this));
                break;
        }

        if (!watcher.objectDestroyed())
            triggerConnectionClosedEvent();
    }

    void triggerConnectionClosedEvent()
    {
        decltype(m_connectionClosedHandlers) connectionClosedHandlers;
        connectionClosedHandlers.swap(m_connectionClosedHandlers);
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
        m_streamSocket->cancelIOSync(nx::network::aio::etTimedOut);
    }
};

} // namespace nx_api
