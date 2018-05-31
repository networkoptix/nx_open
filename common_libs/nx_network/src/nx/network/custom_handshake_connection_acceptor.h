#pragma once

#include <deque>

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
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

    CustomHandshakeConnectionAcceptor(
        std::unique_ptr<AcceptorDelegate> delegate,
        CustomHandshakeConnectionFactory customHandshakeConnection)
        :
        m_delegate(std::move(delegate)),
        m_customHandshakeConnection(std::move(customHandshakeConnection))
    {
        bindToAioThread(m_delegate->getAioThread());
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        m_delegate->bindToAioThread(aioThread);
        m_acceptCallScheduler.bindToAioThread(aioThread);

        for (auto& connection: m_connections)
            connection->bindToAioThread(aioThread);
        for (auto& connection: m_acceptedConnections)
            connection->bindToAioThread(aioThread);
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

        auto connectionToReturn = takeNextAcceptedConnection(lock);
        if (!connectionToReturn)
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

        return connectionToReturn;
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        m_delegate->pleaseStopSync();
        m_acceptCallScheduler.pleaseStopSync();
        m_connections.clear();
        m_acceptedConnections.clear();
    }

private:
    using Connections = std::list<std::unique_ptr<CustomHandshakeConnection>>;

    std::unique_ptr<AcceptorDelegate> m_delegate;
    Connections m_connections;
    AcceptCompletionHandler m_acceptHandler;
    std::deque<std::unique_ptr<CustomHandshakeConnection>> m_acceptedConnections;
    std::size_t m_maxReadyConnectionCount = kDefaultMaxReadyConnectionCount;
    aio::BasicPollable m_acceptCallScheduler;
    mutable QnMutex m_mutex;
    bool m_isDelegateAccepting = false;
    CustomHandshakeConnectionFactory m_customHandshakeConnection;

    void openConnections(const QnMutexLockerBase& /*lock*/)
    {
        using namespace std::placeholders;

        NX_ASSERT(isInSelfAioThread());

        if (m_acceptedConnections.size() + m_connections.size() >=
            m_maxReadyConnectionCount)
        {
            return;
        }

        if (!m_isDelegateAccepting)
        {
            m_delegate->acceptAsync(std::bind(
                &CustomHandshakeConnectionAcceptor::onConnectionAccepted, this, _1, _2));
            m_isDelegateAccepting = true;
        }
    }

    void onConnectionAccepted(
        SystemError::ErrorCode /*systemErrorCode*/,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
        using namespace std::placeholders;

        QnMutexLocker lock(&m_mutex);

        m_isDelegateAccepting = false;

        if (connection && connection->setNonBlockingMode(true))
        {
            connection->bindToAioThread(getAioThread());

            m_connections.push_back(
                m_customHandshakeConnection(std::move(connection)));
            m_connections.back()->handshakeAsync(
                std::bind(&CustomHandshakeConnectionAcceptor::onHandshakeDone, this,
                    std::prev(m_connections.end()), _1));
        }

        openConnections(lock);
    }

    void onHandshakeDone(
        typename Connections::iterator connectionIter,
        SystemError::ErrorCode handshakeResult)
    {
        QnMutexLocker lock(&m_mutex);

        auto connection = std::move(*connectionIter);
        m_connections.erase(connectionIter);
        if (handshakeResult != SystemError::noError)
        {
            openConnections(lock);
            return;
        }

        m_acceptedConnections.push_back(std::move(connection));

        post(
            [this]()
            {
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

        auto connection = takeNextAcceptedConnection(lock);
        openConnections(lock);
        if (connection)
        {
            // TODO: #ak Uncomment, fix & remove extra post from onHandshakeDone.
            //connection->cancelIOSync(aio::etNone);
            connection->bindToAioThread(SocketGlobals::aioService().getRandomAioThread());
            nx::utils::swapAndCall(
                m_acceptHandler, SystemError::noError, std::move(connection));
        }
    }

    std::unique_ptr<CustomHandshakeConnection> takeNextAcceptedConnection(
        const QnMutexLockerBase& /*lock*/)
    {
        std::unique_ptr<CustomHandshakeConnection> connection;
        if (!m_acceptedConnections.empty())
        {
            connection = std::move(m_acceptedConnections.front());
            m_acceptedConnections.pop_front();
        }
        return connection;
    }

    void cancelIoWhileInOwnAioThread()
    {
        m_acceptHandler = nullptr;
        m_acceptCallScheduler.cancelPostedCallsSync();
    }
};

} // namespace network
} // namespace nx
