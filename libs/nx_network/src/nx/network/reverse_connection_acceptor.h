// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <limits>
#include <list>
#include <memory>

#include <boost/container/flat_map.hpp>

#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/progressive_delay_calculator.h>
#include <nx/utils/system_error.h>
#include <nx/utils/thread/mutex.h>

#include "aio/aio_service.h"
#include "aio/basic_pollable.h"
#include "socket_global.h"

namespace nx {
namespace network {

using ReverseConnectionCompletionHandler =
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

class AbstractAcceptableReverseConnection
{
public:
    virtual ~AbstractAcceptableReverseConnection() = default;

    virtual void waitForOriginatorToStartUsingConnection(
        ReverseConnectionCompletionHandler handler) = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * Establishes reverse connections to the originator.
 */
template<typename Connection>
class AbstractReverseConnector:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using ConnectHandler =
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::unique_ptr<Connection>)>;

    virtual ~AbstractReverseConnector() = default;

    /**
     * Invoked on connect completion.
     * Note: number of invocations of this functor may be greater than number of
     * AbstractReverseConnector::connectAsync calls. But, not less.
     */
    virtual void setOnConnect(ConnectHandler handler) = 0;

    /**
     * Initiates new connection.
     */
    virtual void connectAsync() = 0;

    virtual int pendingConnectCount() = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * Implements AbstractReverseConnector implying that the Connection type provides the method
 * <pre><code>
 * void connect(ReverseConnectionCompletionHandler handler);
 * </code></pre>
 * to connect to the reverse connection server.
 */
template<typename Connection>
class SimpleReverseConnector:
    public AbstractReverseConnector<Connection>
{
    using base_type = AbstractReverseConnector<Connection>;

public:
    using ConnectionFactoryFunc =
        nx::utils::MoveOnlyFunc<std::unique_ptr<Connection>()>;

    SimpleReverseConnector(ConnectionFactoryFunc factoryFunc):
        m_factoryFunc(std::move(factoryFunc))
    {
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        std::for_each(
            m_connections.begin(), m_connections.end(),
            [aioThread](auto& c) { c.second->bindToAioThread(aioThread); });
    }

    virtual void setOnConnect(typename base_type::ConnectHandler handler) override
    {
        m_handler = std::move(handler);
    }

    virtual void connectAsync() override
    {
        this->dispatch([this]() { connectAsync(m_factoryFunc()); });
    }

    virtual int pendingConnectCount() override
    {
        return (int) m_connections.size();
    }

protected:
    /**
     * Note: must be invoked in AIO thread.
     */
    void connectAsync(std::unique_ptr<Connection> connection)
    {
        NX_ASSERT(this->isInSelfAioThread());

        connection->bindToAioThread(this->getAioThread());

        connection->connect(
            [this, connection = connection.get()](auto&&... args) {
                reportConnectResult(connection, std::forward<decltype(args)>(args)...);
            });

        m_connections.emplace(connection.get(), std::move(connection));
    }

    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();

        m_connections.clear();
    }

    void reportConnectResult(
        Connection* connectionPtr,
        SystemError::ErrorCode resultCode)
    {
        auto it = m_connections.find(connectionPtr);
        if (it == m_connections.end())
            return m_handler(SystemError::invalidData, nullptr);

        auto connection = std::exchange(it->second, nullptr);
        m_connections.erase(it);

        m_handler(resultCode, std::move(connection));
    }

private:
    ConnectionFactoryFunc m_factoryFunc;
    typename base_type::ConnectHandler m_handler;
    boost::container::flat_map<Connection*, std::unique_ptr<Connection>> m_connections;
};

//-------------------------------------------------------------------------------------------------

struct ConnectRetryPolicy
{
    /**
     * This number of subsequent connect failures is ignored.
     * If failures continue then each next connection attempt is done accordingly to the delayPolicy.
     * Each successful connection resets current failure count and the delay calculator.
     */
    int ignoredFailureCount = 3;

    nx::utils::ProgressiveDelayPolicy delayPolicy =
        nx::utils::ProgressiveDelayPolicy(
            /*initialDelay*/ std::chrono::milliseconds(100), /*delayMultiplier*/ 2,
            /*maxDelay*/ std::chrono::seconds(11), /*delayDistributionRatio*/ 0);
};

/**
 * Server-side implementation of "reverse connection" technique.
 * That means: server establishes connection(s) to the client and waits for client
 *   to begin using that connection.
 * When client begins using connection, this class provides connection to the caller
 *   by acceptAsync or getNextConnectionIfAny methods.
 *
 * Connection is ready to be accepted when it has triggered handler
 *   passed to AbstractAcceptableReverseConnection::waitForOriginatorToStartUsingConnection
 *   with SystemError::noError code.
 * Always keeps specified number of connections alive.
 * @param Connection MUST implement
 *   AbstractAcceptableReverseConnection and aio::BasicPollable.
 * NOTE: This class follows behavior of AbstractStreamSocketAcceptor, though not implements it.
 * NOTE: Does not implement accept timeout. If you need it, consider using aio::Timer.
 * NOTE: If AbstractAcceptableReverseConnection::waitForOriginatorToStartUsingConnection reports error,
 *   that error is passed to the scheduled acceptAsync call.
 */
template<typename Connection>
class ReverseConnectionAcceptor:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using AcceptCompletionHandler = nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, std::unique_ptr<Connection>)>;

    using ConnectErrorHandler = nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, std::unique_ptr<Connection>)>;

    using OnConnectionEstablished = nx::utils::MoveOnlyFunc<
        void(const Connection& connection)>;

    using ConnectionFactoryFunc =
        nx::utils::MoveOnlyFunc<std::unique_ptr<Connection>()>;

    constexpr static std::size_t kDefaultPreemptiveConnectionCount = 7;
    constexpr static std::size_t kDefaultMaxReadyConnectionCount = 32;

    ReverseConnectionAcceptor(std::unique_ptr<AbstractReverseConnector<Connection>> connector):
        m_connector(std::move(connector)),
        m_connectRetryDelayCalculator(m_connectRetryPolicy.delayPolicy)
    {
        bindToAioThread(getAioThread());

        m_connector->setOnConnect(
            [this](auto&&... args) { onConnectDone(std::forward<decltype(args)>(args)...); });
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        m_connector->bindToAioThread(aioThread);
        m_acceptCallScheduler.bindToAioThread(aioThread);
        m_connectRetryTimer.bindToAioThread(aioThread);

        for (auto& connectionContext: m_connections)
            connectionContext.connection->bindToAioThread(aioThread);
        for (auto& connection: m_acceptedConnections)
            connection->bindToAioThread(aioThread);
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

    /**
     * The connection retry policy affects delay before the next connection attempt after connect failure.
     * For default values see ConnectRetryPolicy.
     */
    void setConnectRetryPolicy(ConnectRetryPolicy policy)
    {
        m_connectRetryPolicy = policy;
        m_connectRetryDelayCalculator = nx::utils::ProgressiveDelayCalculator(m_connectRetryPolicy.delayPolicy);
    }

    /**
     * if m_connector->connectAsync() fails, this handler is invoked with the failed connection
     * instead of asking the connection to start accepting.
     */
    void setConnectErrorHandler(ConnectErrorHandler handler)
    {
        m_connectErrorHandler = std::move(handler);
    }

    void start()
    {
        post([this]() { openConnections(/*causedByConnectError?*/ false); });
    }

    void acceptAsync(AcceptCompletionHandler handler)
    {
        m_acceptCallScheduler.post(
            [this, handler = std::move(handler)]() mutable
            {
                m_acceptHandler = std::move(handler);

                auto connectionToReturn = takeNextAcceptedConnection();
                if (!connectionToReturn)
                    return;

                startAcceptingAnotherConnectionIfAppropriate();
                provideConnectionToTheCaller(std::move(connectionToReturn));
            });
    }

    void cancelIOSync()
    {
        executeInAioThreadSync([this]() { cancelIoWhileInOwnAioThread(); });
    }

    /**
     * @return null if connection could not been accepted.
     * Use SystemError::getLastOsErrorCode() to get error code.
     * If accepted connections queue is empty, error code is set to SystemError::wouldBlock.
     */
    std::unique_ptr<Connection> getNextConnectionIfAny()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto connectionToReturn = takeNextAcceptedConnection();
        if (!connectionToReturn)
        {
            SystemError::setLastErrorCode(SystemError::wouldBlock);
            return nullptr;
        }

        startAcceptingAnotherConnectionIfAppropriate();
        return connectionToReturn;
    }

    void setOnConnectionEstablished(
        OnConnectionEstablished handler)
    {
        m_onConnectionEstablished = std::move(handler);
    }

    AbstractReverseConnector<Connection>& connector() { return *m_connector; }

protected:
    virtual void stopWhileInAioThread() override
    {
        m_acceptCallScheduler.pleaseStopSync();
        m_connectRetryTimer.pleaseStopSync();
        m_connections.clear();
        m_acceptedConnections.clear();
        m_connector.reset();
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
        std::unique_ptr<Connection> connection;
        ConnectionState state;

        ConnectionContext():
            state(ConnectionState::connecting)
        {
        }

        ConnectionContext(std::unique_ptr<Connection> connection):
            connection(std::move(connection)),
            state(ConnectionState::connecting)
        {
        }
    };

    using Connections = std::list<ConnectionContext>;

    std::unique_ptr<AbstractReverseConnector<Connection>> m_connector;
    Connections m_connections;
    AcceptCompletionHandler m_acceptHandler;
    ConnectErrorHandler m_connectErrorHandler;
    OnConnectionEstablished m_onConnectionEstablished;
    std::deque<std::unique_ptr<Connection>> m_acceptedConnections;
    std::size_t m_preemptiveConnectionCount = kDefaultPreemptiveConnectionCount;
    std::size_t m_maxReadyConnectionCount = kDefaultMaxReadyConnectionCount;
    ConnectRetryPolicy m_connectRetryPolicy;
    nx::utils::ProgressiveDelayCalculator m_connectRetryDelayCalculator;
    aio::BasicPollable m_acceptCallScheduler;
    mutable nx::Mutex m_mutex;
    int m_subSequentErrorCount = 0;
    aio::Timer m_connectRetryTimer;
    bool m_connectRetryTimerStarted = false;

    void openConnections(bool causedByConnectError)
    {
        NX_ASSERT(isInSelfAioThread());

        if (causedByConnectError)
        {
            ++m_subSequentErrorCount;
        }
        else if (m_subSequentErrorCount > 0)
        {
            NX_VERBOSE(this, "Resuming normal operation after failure");
            m_subSequentErrorCount = 0;
            m_connectRetryDelayCalculator.reset();
        }

        if (m_connectRetryTimerStarted)
            return;

        std::optional<std::chrono::milliseconds> delay;
        if (m_subSequentErrorCount > m_connectRetryPolicy.ignoredFailureCount)
        {
            delay = m_connectRetryDelayCalculator.calculateNewDelay();
            NX_VERBOSE(this, "Delaying the next connection attempt for %1", delay);
        }

        if (delay)
        {
            m_connectRetryTimer.start(*delay, [this]() {
                m_connectRetryTimerStarted = false;
                openConnections(/*max connections to open*/ 1);
            });
            m_connectRetryTimerStarted = true;
            return;
        }

        openConnections();
    }

    void openConnections(int limit = std::numeric_limits<int>::max())
    {
        using namespace std::placeholders;

        const auto count = m_connections.size() + m_connector->pendingConnectCount();
        for (int i = 0;
            m_connections.size() + m_connector->pendingConnectCount() < m_preemptiveConnectionCount
                && i < limit;
            ++i)
        {
            m_connector->connectAsync();
        }

        if (m_connections.size() > count)
        {
            NX_VERBOSE(this, "Added %1 connection(s). Total %2",
                m_connections.size() - count, m_connections.size());
        }
    }

    void onConnectDone(
        SystemError::ErrorCode connectResult,
        std::unique_ptr<Connection> connection)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (connectResult != SystemError::noError)
        {
            NX_VERBOSE(this, "Connect failed. %1", connectResult);

            if (m_connectErrorHandler)
                m_connectErrorHandler(connectResult, std::move(connection));

            return openConnections(/*causedByConnectError?*/ true);
        }

        m_connections.push_back(ConnectionContext());
        auto connectionIter = --m_connections.end();
        connectionIter->connection = std::move(connection);
        connectionIter->state = ConnectionState::connected;

        // There may not be enough connections if failure has happened recently.
        openConnections(/*causedByConnectError?*/ false);

        if (m_onConnectionEstablished)
            m_onConnectionEstablished(*connectionIter->connection);

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

        connectionIter->connection->waitForOriginatorToStartUsingConnection(
            std::bind(&ReverseConnectionAcceptor::onConnectionReadyForUsage, this,
                connectionIter, std::placeholders::_1));
    }

    void onConnectionReadyForUsage(
        typename Connections::iterator connectionIter,
        SystemError::ErrorCode resultCode)
    {
        NX_VERBOSE(this, "Reverse connection %1 preparation completed with result %2",
            &connectionIter->connection, SystemError::toString(resultCode));

        std::unique_ptr<Connection> connectionToReturn;

        {
            NX_MUTEX_LOCKER lock(&m_mutex);

            if (resultCode == SystemError::noError)
                m_acceptedConnections.push_back(std::move(connectionIter->connection));

            m_connections.erase(connectionIter);
            openConnections(/*causedByConnectError?*/ false);

            if (m_acceptHandler)
                connectionToReturn = takeNextAcceptedConnection();

            startAcceptingAnotherConnectionIfAppropriate();
        }

        if (connectionToReturn)
            provideConnectionToTheCaller(std::move(connectionToReturn));
    }

    std::unique_ptr<Connection> takeNextAcceptedConnection()
    {
        std::unique_ptr<Connection> result;
        if (!m_acceptedConnections.empty())
        {
            result = std::move(m_acceptedConnections.front());
            m_acceptedConnections.pop_front();

            NX_VERBOSE(this, "Providing reverse connection %1", &result);
        }
        return result;
    }

    void startAcceptingAnotherConnectionIfAppropriate()
    {
        if (m_acceptedConnections.size() + connectionsBeingAcceptedCount() <
            m_maxReadyConnectionCount)
        {
            auto connectedConnectionIter = getAnyConnectedConnection();
            if (connectedConnectionIter != m_connections.end())
                startAccepting(connectedConnectionIter);
        }
    }

    void provideConnectionToTheCaller(
        std::unique_ptr<Connection> connection)
    {
        connection->bindToAioThread(SocketGlobals::aioService().getRandomAioThread());

        nx::utils::swapAndCall(
            m_acceptHandler,
            SystemError::noError,
            std::move(connection));
    }

    typename Connections::iterator getAnyConnectedConnection()
    {
        return std::find_if(
            m_connections.begin(), m_connections.end(),
            [](const ConnectionContext& val) { return val.state == ConnectionState::connected; });
    }

    void cancelIoWhileInOwnAioThread()
    {
        m_acceptHandler = nullptr;
        m_acceptCallScheduler.cancelPostedCallsSync();
    }
};

} // namespace network
} // namespace nx
