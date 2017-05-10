#pragma once

#include <deque>
#include <list>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "aio/basic_pollable.h"

namespace nx {
namespace network {

using ReverseConnectionCompletionHandler = 
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

class AbstractAcceptableReverseConnection
{
public:
    virtual ~AbstractAcceptableReverseConnection() = default;

    virtual void connectAsync(
        ReverseConnectionCompletionHandler handler) = 0;
    virtual void waitForConnectionToBeReadyAsync(
        ReverseConnectionCompletionHandler handler) = 0;
};

/**
 * Server-side implementation of "reverse connection" technique.
 * That means: server establishes connection(s) to the client and waits for client
 *   to begin using that connection. 
 * When client begins using connection, this class provides connection to the caller 
 *   by acceptAsync or getNextConnectionIfAny methods.
 *
 * Connection is ready to be accepted when it has triggered handler 
 *   passed to AbstractAcceptableReverseConnection::waitForConnectionToBeReadyAsync 
 *   with SystemError::noError code.
 * Always keeps specified number of connections alive.
 * @param AcceptableReverseConnection MUST implement 
 *   AbstractAcceptableReverseConnection and aio::BasicPollable.
 * NOTE: This class follows behavior of AbstractAcceptor, though not implements it.
 * NOTE: Does not implement accept timeout. If you need it, consider using aio::Timer.
 * NOTE: If AbstractAcceptableReverseConnection::waitForConnectionToBeReadyAsync reports error, 
 *   that error is passed to the scheduled acceptAsync call.
 */
template<typename AcceptableReverseConnection>
class ReverseConnectionAcceptor:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using AcceptCompletionHandler = 
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, 
                std::unique_ptr<AcceptableReverseConnection>)>;
    
    using ConnectionFactoryFunc = 
        nx::utils::MoveOnlyFunc<std::unique_ptr<AcceptableReverseConnection>()>;

    constexpr static std::size_t kDefaultPreemptiveConnectionCount = 7;
    constexpr static std::size_t kDefaultMaxReadyConnectionCount = 32;

    /**
     * @param connectionFactory connectionFactory() must return 
     *   std::unique_ptr<AcceptableReverseConnection>.
     */
    ReverseConnectionAcceptor(ConnectionFactoryFunc connectionFactory):
        m_connectionFactory(std::move(connectionFactory))
    {
    }

    void setPreemptiveConnectionCount(std::size_t count)
    {
        m_preemptiveConnectionCount = count;
    }

    std::size_t preemptiveConnectionCount() const
    {
        return m_preemptiveConnectionCount;
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
                openConnections();
            });
    }

    void acceptAsync(AcceptCompletionHandler handler)
    {
        post(
            [this, handler = std::move(handler)]() mutable
            {
                m_acceptHandler = std::move(handler);
                if (!m_acceptedConnections.empty())
                    provideNextConnection();
            });
    }

    void cancelIOSync()
    {
        if (isInSelfAioThread())
        {
            cancelIoWhileInOwnAioThread();
        }
        else
        {
            nx::utils::promise<void> done;
            post(
                [this, &done]()
                {
                    cancelIoWhileInOwnAioThread();
                    done.set_value();
                });
            done.get_future().wait();
        }
    }

    std::unique_ptr<AcceptableReverseConnection> getNextConnectionIfAny()
    {
        // TODO
        return nullptr;
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        m_connections.clear();
        m_acceptedConnections.clear();
    }

private:
    enum class ConnectionState
    {
        connecting,
        connected,
        accepting,
    };

    struct ConnectionContext
    {
        std::unique_ptr<AcceptableReverseConnection> connection;
        ConnectionState state;

        ConnectionContext():
            state(ConnectionState::connecting)
        {
        }

        ConnectionContext(std::unique_ptr<AcceptableReverseConnection> connection):
            connection(std::move(connection)),
            state(ConnectionState::connecting)
        {
        }
    };

    using Connections = std::list<ConnectionContext>;

    ConnectionFactoryFunc m_connectionFactory;
    Connections m_connections;
    AcceptCompletionHandler m_acceptHandler;
    std::deque<std::unique_ptr<AcceptableReverseConnection>> m_acceptedConnections;
    std::size_t m_preemptiveConnectionCount = kDefaultPreemptiveConnectionCount;
    std::size_t m_maxReadyConnectionCount = kDefaultMaxReadyConnectionCount;

    void openConnections()
    {
        using namespace std::placeholders;

        NX_ASSERT(isInSelfAioThread());

        while (m_connections.size() < m_preemptiveConnectionCount)
        {
            auto connection = m_connectionFactory();
            m_connections.push_back(ConnectionContext());
            connection->connectAsync(
                std::bind(&ReverseConnectionAcceptor::onConnectDone, this,
                    --m_connections.end(), _1));
            m_connections.back().connection = std::move(connection);
        }
    }

    void onConnectDone(
        typename Connections::iterator connectionIter,
        SystemError::ErrorCode connectResult)
    {
        if (connectResult != SystemError::noError)
        {
            m_connections.erase(connectionIter);
            openConnections();
            return;
        }

        connectionIter->state = ConnectionState::connected;

        if (m_acceptedConnections.size() + connectionsBeingAcceptedCount() >= 
            m_maxReadyConnectionCount)
        {
            // Waiting for some room in m_acceptedConnections.
            return;
        }

        startAccepting(connectionIter);
    }

    std::size_t connectionsBeingAcceptedCount() const
    {
        return std::count_if(
            m_connections.begin(), m_connections.end(),
            [](const ConnectionContext& val) { return val.state == ConnectionState::accepting; });
    }

    void startAccepting(typename Connections::iterator connectionIter)
    {
        connectionIter->state = ConnectionState::accepting;

        connectionIter->connection->waitForConnectionToBeReadyAsync(
            std::bind(&ReverseConnectionAcceptor::onWaitForReadinessCompleted, this,
                connectionIter, std::placeholders::_1));
    }

    void onWaitForReadinessCompleted(
        typename Connections::iterator connectionIter,
        SystemError::ErrorCode resultCode)
    {
        if (resultCode != SystemError::noError)
        {
            auto connectedConnectionIter = getAnyConnectedConnection();
            if (connectionIter != m_connections.end())
                startAccepting(connectionIter);
        }
        else
        {
            auto connection = std::move(connectionIter->connection);
            saveAcceptedConnection(std::move(connection));
        }

        m_connections.erase(connectionIter);
        openConnections();
    }

    typename Connections::iterator getAnyConnectedConnection()
    {
        return std::find_if(
            m_connections.begin(), m_connections.end(),
            [](const ConnectionContext& val) { return val.state == ConnectionState::connected; });
    }

    void saveAcceptedConnection(
        std::unique_ptr<AcceptableReverseConnection> connection)
    {
        m_acceptedConnections.push_back(std::move(connection));
        if (m_acceptHandler)
            provideNextConnection();
    }

    void provideNextConnection()
    {
        auto connection = std::move(m_acceptedConnections.front());
        m_acceptedConnections.pop_front();

        auto connectionIter = getAnyConnectedConnection();
        if (connectionIter != m_connections.end())
            startAccepting(connectionIter);

        nx::utils::swapAndCall(
            m_acceptHandler,
            SystemError::noError,
            std::move(connection));
    }

    void cancelIoWhileInOwnAioThread()
    {
        m_acceptHandler = nullptr;
    }
};

} // namespace network
} // namespace nx
