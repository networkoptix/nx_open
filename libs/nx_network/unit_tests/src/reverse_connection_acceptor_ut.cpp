// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <set>
#include <tuple>

#include <gtest/gtest.h>

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/reverse_connection_acceptor.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::test {

namespace {

class ReverseConnection;

class ReverseConnectionPool
{
public:
    void afterCreation(ReverseConnection* connection)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_connections.insert(connection);
    }

    void beforeDestruction(ReverseConnection* connection)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_connections.erase(connection);
    }

    bool empty() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_connections.empty();
    }

    const ReverseConnection* front() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return *m_connections.begin();
    }

    ReverseConnection* front()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return *m_connections.begin();
    }

    std::size_t size() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_connections.size();
    }

private:
    mutable nx::Mutex m_mutex;
    std::set<ReverseConnection*> m_connections;
};

//-------------------------------------------------------------------------------------------------

class ReverseConnection:
    public aio::BasicPollable,
    public AbstractAcceptableReverseConnection
{
public:
    ReverseConnection(ReverseConnectionPool* pool):
        m_pool(pool)
    {
        m_pool->afterCreation(this);
    }

    ~ReverseConnection()
    {
        if (m_onBeforeDestruction)
            m_onBeforeDestruction();
        m_pool->beforeDestruction(this);
    }

    void connect(
        ReverseConnectionCompletionHandler handler)
    {
        post(
            [this, handler = std::move(handler)]()
            {
                m_isConnected = true;
                const auto resultCode = m_connectResult ? *m_connectResult : SystemError::noError;
                if (m_onConnectDone)
                    m_onConnectDone(resultCode);
                handler(resultCode);
            });
    }

    virtual void waitForOriginatorToStartUsingConnection(
        ReverseConnectionCompletionHandler handler) override
    {
        post(
            [this, handler = std::move(handler)]() mutable
            {
                m_connectionBecameReadyHandler = std::move(handler);
                m_waitForReadinessInvoked = true;
                if (m_ready)
                    reportReadiness();

            });
    }

    bool isConnected() const
    {
        return m_isConnected;
    }

    bool isWaitForReadinessInvoked() const
    {
        return m_waitForReadinessInvoked;
    }

    void makeReady()
    {
        post(
            [this]() mutable
            {
                m_ready = true;
                if (m_connectionBecameReadyHandler)
                    reportReadiness();
            });
    }

    void setOnConnectDone(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        m_onConnectDone = std::move(handler);
    }

    void setOnReadinessWaitDone(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        m_onReadinessWaitDone = std::move(handler);
    }

    void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_onBeforeDestruction = std::move(handler);
    }

    void setConnectResult(SystemError::ErrorCode resultCode)
    {
        m_connectResult = resultCode;
    }

    void setReadinessWaitResult(SystemError::ErrorCode resultCode)
    {
        m_readinessWaitResult = resultCode;
        makeReady();
    }

private:
    ReverseConnectionPool* m_pool = nullptr;
    ReverseConnectionCompletionHandler m_connectionBecameReadyHandler;
    bool m_isConnected = false;
    bool m_ready = false;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onConnectDone;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onReadinessWaitDone;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestruction;
    std::optional<SystemError::ErrorCode> m_connectResult;
    std::optional<SystemError::ErrorCode> m_readinessWaitResult;
    bool m_waitForReadinessInvoked = false;

    void reportReadiness()
    {
        const auto result =
            m_readinessWaitResult ? *m_readinessWaitResult : SystemError::noError;
        if (m_onReadinessWaitDone)
            m_onReadinessWaitDone(result);
        nx::utils::swapAndCall(m_connectionBecameReadyHandler, result);
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class ReverseConnectionAcceptor:
    public ::testing::Test
{
public:
    using TestReverseConnectionAcceptor = network::ReverseConnectionAcceptor<ReverseConnection>;

    ReverseConnectionAcceptor():
        m_preemptiveConnectionCount(nx::utils::random::number<std::size_t>(3, 11)),
        m_acceptor(std::make_unique<SimpleReverseConnector<ReverseConnection>>(
            std::bind(&ReverseConnectionAcceptor::createReverseConnection, this)))
    {
        m_acceptor.setPreemptiveConnectionCount(m_preemptiveConnectionCount);
    }

    ~ReverseConnectionAcceptor()
    {
        m_acceptor.pleaseStopSync();
    }

protected:
    void setConnectionAlwaysFails(bool val)
    {
        m_connectResult = val ? SystemError::connectionRefused : SystemError::noError;
    }

    void setReadinessWaitAlwaysFails(bool val)
    {
        m_readinessWaitResult = val ? SystemError::connectionRefused : SystemError::noError;
    }

    void setEveryConnectionBecomesReadyAtOnce(bool val)
    {
        m_everyConnectionBecomesReadyAtOnce = val;
    }

    void setConnectRetryPolicy(ConnectRetryPolicy policy)
    {
        m_acceptor.setConnectRetryPolicy(policy);
    }

    void givenStartedAcceptor()
    {
        m_acceptor.start();
    }

    void givenReadyConnection()
    {
        givenStartedAcceptor();
        whenConnectionBecameReady();
    }

    void givenStartedAcceptAsync()
    {
        givenStartedAcceptor();
        whenInvokedAcceptAsync();
    }

    void whenInvokedAcceptAsync()
    {
        using namespace std::placeholders;

        m_acceptor.acceptAsync(
            std::bind(&ReverseConnectionAcceptor::onAccepted, this, _1, _2));
    }

    void whenConnectionBecameReady()
    {
        thenConnectionToTheTargetHasBeenOpened();
        m_reverseConnectionPool.front()->makeReady();
    }

    void whenConnectFails()
    {
        m_connectionsFailedToConnect.pop();
    }

    void whenConnectSucceeds()
    {
        m_connectedConnections.pop();
    }

    void whenReadinessWaitFails()
    {
        m_failedConnections.pop();
    }

    void whenConnectionCountHasReachedMaximum()
    {
        const auto desiredConnectionCount =
            m_acceptor.readyConnectionQueueSize() + m_acceptor.preemptiveConnectionCount();

        while (m_reverseConnectionPool.size() < desiredConnectionCount)
            std::this_thread::yield();
    }

    void whenCancelledIo()
    {
        m_acceptor.cancelIOSync();
    }

    void whenCancelledIoWithinAioThread()
    {
        nx::utils::promise<void> done;

        m_acceptor.post(
            [&done, this]()
            {
                m_acceptor.cancelIOSync();
                done.set_value();
            });
        done.get_future().wait();
    }

    void whenGettingConnection()
    {
        auto connection = m_acceptor.getNextConnectionIfAny();
        m_acceptedConnections.push(std::make_tuple(
            SystemError::getLastOSErrorCode(),
            std::move(connection)));
    }

    void whenConnectFailureCountReaches(int count)
    {
        for (int i = 0; i < count; ++i)
            m_connectionsFailedToConnect.pop();
    }

    void thenIoEventIsNotDelivered()
    {
        constexpr std::chrono::milliseconds delay(100);

        whenConnectionBecameReady();
        ASSERT_FALSE(m_acceptedConnections.pop(delay));
    }

    void thenConnectionToTheTargetHasBeenOpened()
    {
        while (m_reverseConnectionPool.empty() ||
            !m_reverseConnectionPool.front()->isConnected())
        {
            std::this_thread::yield();
        }
    }

    void thenPreemptiveNumberOfConnectionsHaveBeenOpened()
    {
        while (m_reverseConnectionPool.size() != m_preemptiveConnectionCount)
            std::this_thread::yield();
    }

    void thenConnectionHasBeenAccepted()
    {
        auto acceptResult = m_acceptedConnections.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(acceptResult));
        ASSERT_NE(nullptr, std::get<1>(acceptResult));
    }

    void thenConnectionIsRemoved()
    {
        m_removedConnections.pop();
    }

    void thenAcceptorWaitsForConnectionToBecomeReady()
    {
        while (!m_reverseConnectionPool.front()->isWaitForReadinessInvoked())
            std::this_thread::yield();
    }

    void thenNoMoreConnectionsAreCreated()
    {
        const auto delay = std::chrono::milliseconds(100);

        const auto poolSize = m_reverseConnectionPool.size();
        std::this_thread::sleep_for(delay);
        ASSERT_EQ(poolSize, m_reverseConnectionPool.size());
    }

    void thenConnectionIsReturnedWithoutBlocking()
    {
        std::unique_ptr<ReverseConnection> connection;
        while (!(connection = m_acceptor.getNextConnectionIfAny()))
            std::this_thread::yield();

        const auto resultCode = connection
            ? SystemError::noError
            : SystemError::getLastOSErrorCode();
        m_acceptedConnections.push(std::make_tuple(
            resultCode,
            std::move(connection)));

        thenConnectionHasBeenAccepted();
    }

    void thenNoConnectionIsReturned()
    {
        if (!m_lastAcceptedConnection)
            m_lastAcceptedConnection = m_acceptedConnections.pop();
        ASSERT_EQ(nullptr, std::get<1>(*m_lastAcceptedConnection));
    }

    void thenErrorCodeIs(SystemError::ErrorCode sysErrorCode)
    {
        if (!m_lastAcceptedConnection)
            m_lastAcceptedConnection = m_acceptedConnections.pop();
        ASSERT_EQ(sysErrorCode, std::get<0>(*m_lastAcceptedConnection));
    }

    void thenNoFutherConnectionAttemptsAreMadeWithin(std::chrono::milliseconds period)
    {
        ASSERT_FALSE(m_connectionsFailedToConnect.pop(period));
    }

    template<typename Func>
    void executeInAcceptorsAioThread(Func func)
    {
        nx::utils::promise<void> done;
        m_acceptor.post(
            [&func, &done]()
            {
                func();
                done.set_value();
            });
        done.get_future().wait();
    }

    std::size_t preemptiveConnectionCount() const { return m_preemptiveConnectionCount; }

protected:
    using AcceptResult =
        std::tuple<SystemError::ErrorCode, std::unique_ptr<ReverseConnection>>;

    const std::size_t m_preemptiveConnectionCount;
    nx::utils::SyncQueue<AcceptResult> m_acceptedConnections;
    std::optional<AcceptResult> m_lastAcceptedConnection;
    ReverseConnectionPool m_reverseConnectionPool;
    TestReverseConnectionAcceptor m_acceptor;
    nx::utils::SyncQueue<ReverseConnection*> m_connectedConnections;
    nx::utils::SyncQueue<ReverseConnection*> m_connectionsFailedToConnect;
    nx::utils::SyncQueue<ReverseConnection*> m_removedConnections;
    nx::utils::SyncQueue<ReverseConnection*> m_readyConnections;
    nx::utils::SyncQueue<ReverseConnection*> m_failedConnections;
    std::optional<SystemError::ErrorCode> m_connectResult;
    std::optional<SystemError::ErrorCode> m_readinessWaitResult;
    bool m_everyConnectionBecomesReadyAtOnce = false;

    std::unique_ptr<ReverseConnection> createReverseConnection()
    {
        auto connection = std::make_unique<ReverseConnection>(&m_reverseConnectionPool);
        connection->setOnConnectDone(
            std::bind(&ReverseConnectionAcceptor::saveConnectResult, this,
                connection.get(), std::placeholders::_1));
        connection->setOnReadinessWaitDone(
            std::bind(&ReverseConnectionAcceptor::saveReadinessWaitResult, this,
                connection.get(), std::placeholders::_1));
        connection->setOnBeforeDestruction(
            std::bind(&ReverseConnectionAcceptor::saveDestroyedConnection, this,
                connection.get()));
        if (m_connectResult)
            connection->setConnectResult(*m_connectResult);
        if (m_readinessWaitResult)
            connection->setReadinessWaitResult(*m_readinessWaitResult);
        if (m_everyConnectionBecomesReadyAtOnce)
            connection->makeReady();
        return connection;
    }

    void saveConnectResult(
        ReverseConnection* connection,
        SystemError::ErrorCode resultCode)
    {
        if (resultCode == SystemError::noError)
            m_connectedConnections.push(connection);
        else
            m_connectionsFailedToConnect.push(connection);
    }

    void saveReadinessWaitResult(
        ReverseConnection* connection,
        SystemError::ErrorCode resultCode)
    {
        if (resultCode == SystemError::noError)
            m_readyConnections.push(connection);
        else
            m_failedConnections.push(connection);
    }

    void saveDestroyedConnection(ReverseConnection* connection)
    {
        m_removedConnections.push(connection);
    }

    void onAccepted(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<ReverseConnection> connection)
    {
        m_acceptedConnections.push(
            std::make_tuple(sysErrorCode, std::move(connection)));
    }
};

TEST_F(ReverseConnectionAcceptor, opens_connection_to_the_target_peer)
{
    givenStartedAcceptor();
    thenConnectionToTheTargetHasBeenOpened();
}

TEST_F(ReverseConnectionAcceptor, preemptive_connection_count_is_followed)
{
    givenStartedAcceptor();
    thenPreemptiveNumberOfConnectionsHaveBeenOpened();
}

TEST_F(ReverseConnectionAcceptor, connection_is_removed_in_case_of_connect_failure)
{
    setConnectionAlwaysFails(true);

    givenStartedAcceptor();
    whenConnectFails();
    thenConnectionIsRemoved();
}

TEST_F(ReverseConnectionAcceptor, waits_for_connection_to_become_ready_after_successful_connect)
{
    givenStartedAcceptor();
    whenConnectSucceeds();
    thenAcceptorWaitsForConnectionToBecomeReady();
}

TEST_F(ReverseConnectionAcceptor, connection_is_removed_in_case_of_wait_failure)
{
    setReadinessWaitAlwaysFails(true);

    givenStartedAcceptor();
    whenReadinessWaitFails();
    thenConnectionIsRemoved();
}

TEST_F(ReverseConnectionAcceptor, ready_connection_is_returned_by_listening_acceptAsync)
{
    givenStartedAcceptor();

    whenInvokedAcceptAsync();
    whenConnectionBecameReady();

    thenConnectionHasBeenAccepted();
}

TEST_F(ReverseConnectionAcceptor, ready_connection_is_peeked_by_acceptAsync)
{
    givenReadyConnection();
    whenInvokedAcceptAsync();
    thenConnectionHasBeenAccepted();
}

TEST_F(ReverseConnectionAcceptor, stops_listening_connections_on_queue_overflow)
{
    setEveryConnectionBecomesReadyAtOnce(true);

    givenStartedAcceptor();
    whenConnectionCountHasReachedMaximum();
    thenNoMoreConnectionsAreCreated();
}

TEST_F(ReverseConnectionAcceptor, cancel_io_in_own_aio_thread)
{
    givenStartedAcceptAsync();
    whenCancelledIoWithinAioThread();
    thenIoEventIsNotDelivered();
}

TEST_F(ReverseConnectionAcceptor, cancel_io_in_other_thread)
{
    givenStartedAcceptAsync();
    whenCancelledIo();
    thenIoEventIsNotDelivered();
}

TEST_F(ReverseConnectionAcceptor, cancel_io_in_own_aio_thread_just_after_scheduling)
{
    givenReadyConnection();

    executeInAcceptorsAioThread(
        [this]()
        {
            whenInvokedAcceptAsync();
            whenCancelledIo();
        });

    thenIoEventIsNotDelivered();
}

TEST_F(ReverseConnectionAcceptor, ready_connection_is_taken_by_getNextConnectionIfAny)
{
    givenReadyConnection();
    thenConnectionIsReturnedWithoutBlocking();
}

TEST_F(ReverseConnectionAcceptor, getNextConnectionIfAny_returns_null_if_no_accepted_connections)
{
    givenStartedAcceptor();

    whenGettingConnection();

    thenNoConnectionIsReturned();
    thenErrorCodeIs(SystemError::wouldBlock);
}

TEST_F(ReverseConnectionAcceptor, new_connect_attempts_are_done_with_delay_after_failure)
{
    setConnectRetryPolicy({3, {std::chrono::hours(24), 1, std::chrono::hours(24), 0}});
    setConnectionAlwaysFails(true);

    givenStartedAcceptor();

    // Acceptor tries to open preemptiveConnectionCount at once, then another 3 until max
    // failure count has been reached.
    whenConnectFailureCountReaches(preemptiveConnectionCount() + 3);

    thenNoFutherConnectionAttemptsAreMadeWithin(std::chrono::seconds(1));
}

TEST_F(ReverseConnectionAcceptor, connect_attempts_are_resumed_after_number_of_failures)
{
    setConnectRetryPolicy({3, {std::chrono::milliseconds(1), 2, std::chrono::seconds(1), 0}});
    setConnectionAlwaysFails(true);
    givenStartedAcceptor();

    whenConnectFailureCountReaches(preemptiveConnectionCount() + 8);
    setConnectionAlwaysFails(false);

    thenPreemptiveNumberOfConnectionsHaveBeenOpened();
}

//-------------------------------------------------------------------------------------------------

class ReverseConnectionAcceptorErrorReporting:
    public ReverseConnectionAcceptor
{
protected:
    virtual void SetUp() override
    {
        m_acceptor.setConnectErrorHandler(
            [this](
                SystemError::ErrorCode sysError,
                std::unique_ptr<ReverseConnection> failedConnection) mutable
            {
                m_connectErrors.push(sysError, std::move(failedConnection));
            });
    }

    virtual void TearDown() override
    {
        m_acceptor.pleaseStopSync();
        deleteFailedConnectionsInOwnThread();
    }

    void deleteFailedConnectionsInOwnThread()
    {
        while (!m_connectErrors.empty())
            deleteConnectionInOwnThread(m_connectErrors.pop().second);
    }

    void deleteConnectionInOwnThread(std::unique_ptr<ReverseConnection> connection)
    {
        auto connectionPtr = connection.get();
        connectionPtr->executeInAioThreadSync(
            [connection = std::move(connection)]() mutable
            {
                connection.reset();
            });
    }

    void thenConnectErrorIsReported()
    {
        auto [sysError, failedConnection] = m_connectErrors.pop();
        ASSERT_NE(SystemError::noError, sysError);
        ASSERT_NE(nullptr, failedConnection);
    }

    void thenWaitForReadyConnectionErrorIsReported()
    {
        auto [sysError, failedConnection] = m_connectErrors.pop();
        ASSERT_NE(SystemError::noError, sysError);
        ASSERT_NE(nullptr, failedConnection);
        ASSERT_TRUE(failedConnection->isConnected());
        ASSERT_TRUE(failedConnection->isWaitForReadinessInvoked());
    }

private:
    nx::utils::SyncMultiQueue<
        SystemError::ErrorCode, std::unique_ptr<ReverseConnection>> m_connectErrors;
};

TEST_F(ReverseConnectionAcceptorErrorReporting, connect_error_is_reported)
{
    setConnectionAlwaysFails(true);

    givenStartedAcceptor();

    whenInvokedAcceptAsync();
    whenConnectFails();

    thenConnectErrorIsReported();
}

//TEST_F(ReverseConnectionAcceptorErrorReporting, connect_error_is_cached_then_reported)
//TEST_F(ReverseConnectionAcceptorErrorReporting, wait_for_originator_event_error_is_cached_then_reported)

TEST_F(ReverseConnectionAcceptorErrorReporting, wait_for_originator_event_error_is_reported)
{
    setReadinessWaitAlwaysFails(true);
    givenStartedAcceptor();

    thenWaitForReadyConnectionErrorIsReported();
}

} // namespace nx::network::test
