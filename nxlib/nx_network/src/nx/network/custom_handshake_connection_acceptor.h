#pragma once

#include <deque>

#include <nx/network/aio/aio_service.h>
#include <nx/network/aio/timer.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

#include "abstract_stream_socket_acceptor.h"

namespace nx {
namespace network {

/**
 * Helper class for implementing acceptors (e.g., StreamServerSocket) of connections
 * that require some custom handshake before being provided to the user.
 * E.g., SSL connection requires handshake before connection can be considered "accepted".
 * So, before returning to the user connection is undergoing some custom handshake.
 */
template<typename AcceptorDelegate, typename CustomHandshakeConnection>
// requires AsyncAcceptor<AcceptorDelegate> &&
//   std::is_base_of<AbstractStreamSocket, CustomHandshakeConnection>::value
class CustomHandshakeConnectionAcceptor:
    public AbstractStreamSocketAcceptor
{
    // TODO: #ak CustomHandshakeConnectionAcceptor should not be a template after inheriting
    //   AbstractStreamServerSocket from AbstractStreamSocketAcceptor.

    using base_type = AbstractStreamSocketAcceptor;

public:
    using CustomHandshakeConnectionFactory =
        nx::utils::MoveOnlyFunc<std::unique_ptr<CustomHandshakeConnection>(
            std::unique_ptr<AbstractStreamSocket>)>;

    constexpr static std::size_t kDefaultMaxReadyConnectionCount = 32;
    // 24 seconds is a default TCP handshake timeout in Linux.
    constexpr static std::chrono::seconds kDefaultHandshakeTimeout =
        std::chrono::seconds(24);

    CustomHandshakeConnectionAcceptor(
        std::unique_ptr<AcceptorDelegate> delegate,
        CustomHandshakeConnectionFactory customHandshakeConnectionFactory)
        :
        m_delegate(std::move(delegate)),
        m_customHandshakeConnectionFactory(
            std::move(customHandshakeConnectionFactory))
    {
        bindToAioThread(m_delegate->getAioThread());
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        m_delegate->bindToAioThread(aioThread);
        m_acceptCallScheduler.bindToAioThread(aioThread);

        for (auto& connectionCtx: m_connectionsBeingHandshaked)
        {
            connectionCtx.connection->bindToAioThread(aioThread);
            connectionCtx.handshakeTimer.bindToAioThread(aioThread);
        }
        for (auto& connectionCtx: m_acceptedConnections)
            connectionCtx.connection->bindToAioThread(aioThread);
    }

    /**
     * When ready-to-use connection count is greater or equal to this value,
     * then creation of new connections is stopped until there is more room in the queue.
     */
    void setReadyConnectionQueueSize(std::size_t count)
    {
        m_maxReadyConnectionCount = count;
    }

    std::size_t readyConnectionQueueSize() const
    {
        return m_maxReadyConnectionCount;
    }

    void setHandshakeTimeout(std::chrono::milliseconds timeout)
    {
        m_handshakeTimeout = timeout;
    }

    std::chrono::milliseconds handshakeTimeout() const
    {
        return m_handshakeTimeout;
    }

    void start()
    {
        post(
            [this]()
            {
                QnMutexLocker lock(&m_mutex);
                openConnections(lock);
            });
    }

    virtual void acceptAsync(AcceptCompletionHandler handler) override
    {
        m_acceptCallScheduler.post(
            [this, handler = std::move(handler)]() mutable
            {
                m_acceptHandler = std::move(handler);

                QnMutexLocker lock(&m_mutex);

                if (!m_acceptedConnections.empty())
                    return provideConnectionToTheCallerIfAppropriate(lock);

                openConnections(lock);
            });
    }

    virtual void cancelIOSync() override
    {
        if (isInSelfAioThread())
        {
            cancelIoWhileInOwnAioThread();
        }
        else
        {
            std::promise<void> done;
            post(
                [this, &done]()
                {
                    cancelIoWhileInOwnAioThread();
                    done.set_value();
                });
            done.get_future().wait();
        }
    }

    /**
     * @return null if connection could not been accepted.
     * Use SystemError::getLastOsErrorCode() to get error code.
     * If accepted connections queue is empty, error code is set to SystemError::wouldBlock.
     */
    std::unique_ptr<CustomHandshakeConnection> getNextConnectionIfAny()
    {
        QnMutexLocker lock(&m_mutex);

        auto acceptResult = takeNextAcceptedConnection(lock);
        if (!acceptResult)
        {
            SystemError::setLastErrorCode(SystemError::wouldBlock);
            return nullptr;
        }

        post(
            [this]()
            {
                QnMutexLocker lock(&m_mutex);
                openConnections(lock);
            });

        if (acceptResult->resultCode != SystemError::noError)
            SystemError::setLastErrorCode(acceptResult->resultCode);
        return std::move(acceptResult->connection);
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        m_delegate->pleaseStopSync();
        m_acceptCallScheduler.pleaseStopSync();
        m_connectionsBeingHandshaked.clear();
        m_acceptedConnections.clear();
    }

private:
    struct ConnectionContext
    {
        std::unique_ptr<CustomHandshakeConnection> connection;
        aio::Timer handshakeTimer;

        ConnectionContext(
            std::unique_ptr<CustomHandshakeConnection> connection)
            :
            connection(std::move(connection))
        {
        }
    };

    struct AcceptResult
    {
        SystemError::ErrorCode resultCode;
        std::unique_ptr<CustomHandshakeConnection> connection;

        AcceptResult(
            SystemError::ErrorCode resultCode,
            std::unique_ptr<CustomHandshakeConnection> connection)
            :
            resultCode(resultCode),
            connection(std::move(connection))
        {
        }
    };

    using Connections = std::list<ConnectionContext>;

    std::unique_ptr<AcceptorDelegate> m_delegate;
    Connections m_connectionsBeingHandshaked;
    AcceptCompletionHandler m_acceptHandler;
    std::deque<AcceptResult> m_acceptedConnections;
    std::size_t m_maxReadyConnectionCount = kDefaultMaxReadyConnectionCount;
    std::chrono::milliseconds m_handshakeTimeout = kDefaultHandshakeTimeout;
    aio::BasicPollable m_acceptCallScheduler;
    mutable QnMutex m_mutex;
    bool m_isDelegateAccepting = false;
    CustomHandshakeConnectionFactory m_customHandshakeConnectionFactory;

    void openConnections(const QnMutexLockerBase& /*lock*/)
    {
        using namespace std::placeholders;

        NX_ASSERT(isInSelfAioThread());

        if (m_acceptedConnections.size() >= m_maxReadyConnectionCount)
            return;

        if (!m_isDelegateAccepting)
        {
            m_delegate->acceptAsync(std::bind(
                &CustomHandshakeConnectionAcceptor::onConnectionAccepted, this, _1, _2));
            m_isDelegateAccepting = true;
        }
    }

    void onConnectionAccepted(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
        using namespace std::placeholders;

        QnMutexLocker lock(&m_mutex);

        m_isDelegateAccepting = false;

        // Not forwarding timedOut since user may not even called
        // CustomHandshakeConnectionAcceptor::acceptAsync.
        // So, he should not receive timedOut right after calling acceptAsync.
        if (systemErrorCode != SystemError::noError &&
            systemErrorCode != SystemError::timedOut)
        {
            m_acceptedConnections.emplace_back(systemErrorCode, nullptr);

            post(
                [this]()
                {
                    QnMutexLocker lock(&m_mutex);
                    provideConnectionToTheCallerIfAppropriate(lock);
                });
        }
        else if (connection && connection->setNonBlockingMode(true))
        {
            connection->bindToAioThread(getAioThread());

            m_connectionsBeingHandshaked.emplace_back(
                m_customHandshakeConnectionFactory(std::move(connection)));
            m_connectionsBeingHandshaked.back().handshakeTimer.bindToAioThread(getAioThread());

            auto handshakeDone =
                std::bind(&CustomHandshakeConnectionAcceptor::onHandshakeDone, this,
                    std::prev(m_connectionsBeingHandshaked.end()), _1);

            m_connectionsBeingHandshaked.back().handshakeTimer.start(
                m_handshakeTimeout,
                std::bind(handshakeDone, SystemError::timedOut));
            m_connectionsBeingHandshaked.back().connection->handshakeAsync(handshakeDone);
        }

        openConnections(lock);
    }

    void onHandshakeDone(
        typename Connections::iterator connectionIter,
        SystemError::ErrorCode handshakeResult)
    {
        QnMutexLocker lock(&m_mutex);

        auto connection = std::move(connectionIter->connection);
        m_connectionsBeingHandshaked.erase(connectionIter);
        if (handshakeResult != SystemError::noError)
        {
            openConnections(lock);
            return;
        }

        const auto aioThread = SocketGlobals::aioService().getCurrentAioThread();
        NX_ASSERT(aioThread == connection->getAioThread());

        m_acceptedConnections.emplace_back(
            SystemError::noError, std::move(connection));

        post(
            [this, aioThread]()
            {
                NX_ASSERT(aioThread == SocketGlobals::aioService().getCurrentAioThread());

                QnMutexLocker lock(&m_mutex);
                provideConnectionToTheCallerIfAppropriate(lock);
            });
    }

    void provideConnectionToTheCallerIfAppropriate(
        const QnMutexLockerBase& lock)
    {
        NX_ASSERT(isInSelfAioThread());

        if (!m_acceptHandler)
            return;

        auto acceptResult = takeNextAcceptedConnection(lock);
        openConnections(lock);
        if (acceptResult)
        {
            if (acceptResult->connection)
            {
                acceptResult->connection->bindToAioThread(
                    SocketGlobals::aioService().getRandomAioThread());
            }

            nx::utils::swapAndCall(
                m_acceptHandler,
                acceptResult->resultCode,
                std::move(acceptResult->connection));
        }
    }

    std::optional<AcceptResult> takeNextAcceptedConnection(
        const QnMutexLockerBase& /*lock*/)
    {
        std::optional<AcceptResult> acceptResult;
        if (!m_acceptedConnections.empty())
        {
            acceptResult = std::move(m_acceptedConnections.front());
            m_acceptedConnections.pop_front();
        }

        return acceptResult;
    }

    void cancelIoWhileInOwnAioThread()
    {
        m_acceptHandler = nullptr;
        m_acceptCallScheduler.cancelPostedCallsSync();
    }
};

} // namespace network
} // namespace nx
