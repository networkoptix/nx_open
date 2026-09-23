// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <nx/network/aio/aio_service.h>
#include <nx/network/aio/timer.h>
#include <nx/network/debug/object_instance_counter.h>
#include <nx/network/socket_global.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/counter.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "abstract_stream_socket_acceptor.h"

namespace nx::network {

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
    // TODO: #akolesnikov CustomHandshakeConnectionAcceptor should not be a template after inheriting
    //   AbstractStreamServerSocket from AbstractStreamSocketAcceptor.

    using base_type = AbstractStreamSocketAcceptor;

public:
    using CustomHandshakeConnectionFactory =
        nx::MoveOnlyFunc<std::unique_ptr<CustomHandshakeConnection>(
            std::unique_ptr<AbstractStreamSocket>)>;

    constexpr static std::size_t kDefaultMaxReadyConnectionCount = 32;
    // 24 seconds is a default TCP handshake timeout in Linux.
    constexpr static std::chrono::seconds kDefaultHandshakeTimeout =
        std::chrono::seconds(24);
    // Only there to keep a stalled AIO thread from exhausting the file descriptor limit, so well
    // above any plausible burst of concurrent handshakes.
    constexpr static std::size_t kDefaultMaxConnectionsBeingHandshakedCount = 1024;

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

    ~CustomHandshakeConnectionAcceptor()
    {
        NX_CRITICAL(!SocketGlobals::instance().aioService().isInAnyAioThread());
        this->pleaseStopSync();
        m_startedAsyncHandshakeCancellationsCounter.wait();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        m_delegate->bindToAioThread(aioThread);
        m_acceptCallScheduler.bindToAioThread(aioThread);

        // NOTE: Not binding connections to the specified thread to distribute work between
        // multiple AIO threads.
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

    /**
     * When this many connections are in the handshake, accepting pauses until some of them
     * complete it. Zero means no limit.
     */
    void setMaxConnectionsBeingHandshakedCount(std::size_t count)
    {
        m_maxConnectionsBeingHandshakedCount = count;
    }

    std::size_t maxConnectionsBeingHandshakedCount() const
    {
        return m_maxConnectionsBeingHandshakedCount;
    }

    /**
     * Accepted connections still in the handshake. Not given to the user yet, so not counted by
     * any connection metric.
     */
    std::size_t connectionsBeingHandshakedCount() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_connectionsBeingHandshaked.size();
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
                NX_MUTEX_LOCKER lock(&m_mutex);
                openConnections(lock);
            });
    }

    virtual void acceptAsync(AcceptCompletionHandler handler) override
    {
        m_acceptCallScheduler.post(
            [this, handler = std::move(handler)]() mutable
            {
                m_acceptHandler = std::move(handler);

                NX_MUTEX_LOCKER lock(&m_mutex);

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
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto acceptResult = takeNextAcceptedConnection(lock);
        if (!acceptResult)
        {
            SystemError::setLastErrorCode(SystemError::wouldBlock);
            return nullptr;
        }

        post(
            [this]()
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
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

        m_acceptedConnections.clear();

        cancelHandshakes();
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

    private:
        nx::network::debug::ObjectInstanceCounter<ConnectionContext> m_instanceCounter;
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

    private:
        nx::network::debug::ObjectInstanceCounter<AcceptResult> m_instanceCounter;
    };

    using Connections = std::map<int /*seq*/, std::unique_ptr<ConnectionContext>>;

    nx::utils::AsyncOperationGuard m_guard;
    std::unique_ptr<AcceptorDelegate> m_delegate;
    Connections m_connectionsBeingHandshaked;
    AcceptCompletionHandler m_acceptHandler;
    std::deque<AcceptResult> m_acceptedConnections;
    std::size_t m_maxReadyConnectionCount = kDefaultMaxReadyConnectionCount;
    std::size_t m_maxConnectionsBeingHandshakedCount = kDefaultMaxConnectionsBeingHandshakedCount;
    std::chrono::milliseconds m_handshakeTimeout = kDefaultHandshakeTimeout;
    aio::BasicPollable m_acceptCallScheduler;
    mutable nx::Mutex m_mutex;
    bool m_isDelegateAccepting = false;
    CustomHandshakeConnectionFactory m_customHandshakeConnectionFactory;
    bool m_acceptSuspended = false;
    bool m_handshakeLimitReached = false;
    std::atomic<int> m_connectionSequence = 0;
    nx::utils::Counter m_startedAsyncHandshakeCancellationsCounter;

    void openConnections(const nx::Locker<nx::Mutex>& /*lock*/)
    {
        using namespace std::placeholders;

        NX_ASSERT(isInSelfAioThread());

        if (m_acceptedConnections.size() >= m_maxReadyConnectionCount)
        {
            if (!m_isDelegateAccepting && !m_acceptSuspended)
            {
                NX_VERBOSE(this, "Suspending accepting new connections since there are already %1",
                    m_acceptedConnections.size());
                m_acceptSuspended = true;
            }
            return;
        }

        // A connection in the handshake holds a descriptor until the handshake completes or times
        // out, and both are posted calls on the AIO thread it is bound to. If that thread stalls,
        // neither runs and the descriptor is never released; unbounded, that exhausts the process
        // limit and aborts the whole process instead of degrading this listener. See VMS-62752.
        if (m_maxConnectionsBeingHandshakedCount > 0
            && m_connectionsBeingHandshaked.size() >= m_maxConnectionsBeingHandshakedCount)
        {
            // Logged once per episode: while pinned at the limit, every completed handshake
            // resumes accepting for exactly one connection, so the suspension itself repeats.
            if (!std::exchange(m_handshakeLimitReached, true))
            {
                NX_WARNING(this,
                    "Suspending accepting new connections since %1 connections are "
                    "still performing the handshake",
                    m_connectionsBeingHandshaked.size());
            }
            return;
        }

        if (m_handshakeLimitReached
            && m_connectionsBeingHandshaked.size() < m_maxConnectionsBeingHandshakedCount / 2)
        {
            NX_INFO(this,
                "Connections performing the handshake are down to %1, no longer at the limit",
                m_connectionsBeingHandshaked.size());
            m_handshakeLimitReached = false;
        }

        if (m_acceptSuspended)
            m_acceptSuspended = false;

        if (!m_isDelegateAccepting)
        {
            NX_VERBOSE(this, "Accepting new connection. There are already %1 accepted",
                m_acceptedConnections.size());

            m_delegate->acceptAsync(std::bind(
                &CustomHandshakeConnectionAcceptor::onConnectionAccepted, this, _1, _2));
            m_isDelegateAccepting = true;
        }
    }

    void onConnectionAccepted(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> socket)
    {
        using namespace std::placeholders;

        NX_ASSERT(isInSelfAioThread());

        NX_MUTEX_LOCKER lock(&m_mutex);

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
                    NX_MUTEX_LOCKER lock(&m_mutex);
                    provideConnectionToTheCallerIfAppropriate(lock);
                });
        }
        else if (socket && socket->setNonBlockingMode(true))
        {
            // NOTE: connection is bound to unknown AIO thread here.
            // Leaving it so that multiple AIO threads are loaded by this object.

            const auto seq = ++m_connectionSequence;

            auto ctx = std::make_unique<ConnectionContext>(
                m_customHandshakeConnectionFactory(std::move(socket)));
            auto ctxPtr = ctx.get();

            m_connectionsBeingHandshaked.emplace(seq, std::move(ctx));
            ctxPtr->connection->bindToAioThread(
                SocketGlobals::aioService().getRandomAioThread());
            ctxPtr->handshakeTimer.bindToAioThread(ctxPtr->connection->getAioThread());

            auto handshakeDone =
                std::bind(&CustomHandshakeConnectionAcceptor::onHandshakeDone, this,
                    m_guard.sharedGuard(), seq, ctxPtr, _1);

            ctxPtr->handshakeTimer.start(
                m_handshakeTimeout,
                std::bind(handshakeDone, SystemError::timedOut));
            ctxPtr->connection->handshakeAsync(handshakeDone);

            NX_VERBOSE(this, "Started handshake of connection %1", ctxPtr->connection.get());
        }

        openConnections(lock);
    }

    void onHandshakeDone(
        std::shared_ptr<nx::utils::AsyncOperationGuard::SharedGuard> sharedGuard,
        int seq,
        ConnectionContext* ctxPtr,
        SystemError::ErrorCode handshakeResult)
    {
        // NOTE: We are in connection's AIO thread which may be different from this->getAioThread().
        // That guarantees that the connection context object is alive.
        ctxPtr->handshakeTimer.pleaseStopSync();

        ctxPtr->connection->cancelIOSync(aio::EventType::etAll);

        // Invoking connection->post to make sure the connection has completed its job and can be
        // safely deleted in any thread.
        ctxPtr->connection->post(
            [this, sharedGuard, seq, handshakeResult]()
            {
                const auto lock = sharedGuard->lock();
                if (!lock)
                    return;

                // this->post to switch to the acceptor's AIO thread.
                post(
                    [this, seq, handshakeResult]()
                    {
                        // NOTE: There is no guarantee that ctxPtr is alive.

                        NX_MUTEX_LOCKER lock(&m_mutex);

                        auto connectionIter = m_connectionsBeingHandshaked.find(seq);
                        if (connectionIter == m_connectionsBeingHandshaked.end())
                            return; // Connection has been removed, just ignoring.

                        auto connection = std::move(connectionIter->second->connection);
                        m_connectionsBeingHandshaked.erase(connectionIter);

                        NX_VERBOSE(this, "Handshake completed. Connection %1, result %2",
                            connection.get(), SystemError::toString(handshakeResult));

                        if (handshakeResult != SystemError::noError)
                        {
                            openConnections(lock);

                            // If a nested post completes before the parent (socket) post, it could
                            // delete the socket while its callback is still running. To prevent
                            // this race, schedule the socket deletion via Post too.
                            const auto connectionPtr = connection.get();
                            connectionPtr->post([c = std::move(connection)]() mutable { c.reset(); });
                            return;
                        }

                        m_acceptedConnections.emplace_back(
                            SystemError::noError,
                            std::move(connection));

                        provideConnectionToTheCallerIfAppropriate(lock);
                    });
            });
    }

    void provideConnectionToTheCallerIfAppropriate(
        const nx::Locker<nx::Mutex>& lock)
    {
        NX_ASSERT(isInSelfAioThread());

        if (!m_acceptHandler)
        {
            // The completed handshake freed a slot, so accepting may have to be re-armed even
            // though there is nobody to hand the connection to yet.
            openConnections(lock);
            return;
        }

        auto acceptResult = takeNextAcceptedConnection(lock);
        openConnections(lock);
        if (acceptResult)
        {
            nx::swapAndCall(
                m_acceptHandler,
                acceptResult->resultCode,
                std::move(acceptResult->connection));
        }
    }

    std::optional<AcceptResult> takeNextAcceptedConnection(
        const nx::Locker<nx::Mutex>& /*lock*/)
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

    void cancelHandshakes()
    {
        m_guard.reset();

        // NOTE: Removing connections asynchronously since they live in different AIO threads.
        Connections connectionsBeingHandshaked;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            connectionsBeingHandshaked = std::exchange(m_connectionsBeingHandshaked, {});
        }

        std::map<aio::AbstractAioThread*, std::vector<std::unique_ptr<ConnectionContext>>>
            connectionContextsByAioThread;
        for (auto& [seq, ctx]: connectionsBeingHandshaked)
        {
            connectionContextsByAioThread[ctx->connection->getAioThread()]
                .push_back(std::move(ctx));
        }

        for (auto& [aioThread, connections]: connectionContextsByAioThread)
        {
            aioThread->post(
                nullptr,
                [
                    connections = std::move(connections),
                    guard =
                        m_startedAsyncHandshakeCancellationsCounter.getScopedIncrement()]() mutable
                {
                    connections.clear();
                });
        }
    }
};

} // namespace nx::network
