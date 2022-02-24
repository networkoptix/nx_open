// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/custom_handshake_connection_acceptor.h>
#include <nx/network/ssl/context.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/timer_manager.h>

namespace nx::network::test {

namespace {

class FailingTcpServerSocket:
    public TCPServerSocket
{
public:
    FailingTcpServerSocket(SystemError::ErrorCode systemErrorCode):
        m_systemErrorCode(systemErrorCode)
    {
    }

    virtual void acceptAsync(AcceptCompletionHandler handler) override
    {
        post(
            [this, handler = std::move(handler)]()
            {
                handler(m_systemErrorCode, nullptr);
            });
    }

private:
    const SystemError::ErrorCode m_systemErrorCode;
};

//-------------------------------------------------------------------------------------------------

class TestSocket:
    public ssl::ServerSideStreamSocket
{
    using base_type = ssl::ServerSideStreamSocket;

public:
    using base_type::base_type;

    TestSocket(
        ssl::Context* context,
        std::unique_ptr<AbstractStreamSocket> delegate,
        std::atomic<int>& ctr,
        nx::utils::TimerManager& timerManager,
        std::atomic<bool>& needToDelayHandshake)
        :
        base_type(context, std::move(delegate)),
        m_ctr(ctr),
        m_timerManager(timerManager),
        m_needToDelayHandshake(needToDelayHandshake)
    {
        ++m_ctr;
    }

    ~TestSocket()
    {
        --m_ctr;
    }

    virtual void handshakeAsync(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        m_handshakeThread = getAioThread();
        if (m_needToDelayHandshake)
        {
            utils::TimerId timerId = m_timerManager.addTimer(
                [this, handler = std::move(handler)](utils::TimerId) mutable
                {
                    base_type::handshakeAsync(std::move(handler));
                },
                std::chrono::seconds(10));
            m_timerGuard = utils::TimerManager::TimerGuard(&m_timerManager, timerId);
        }
        else
        {
            base_type::handshakeAsync(std::move(handler));
        }
    }

    aio::AbstractAioThread* handshakeThread() const
    {
        return m_handshakeThread;
    }

private:
    aio::AbstractAioThread* m_handshakeThread = nullptr;
    std::atomic<int>& m_ctr;
    nx::utils::TimerManager& m_timerManager;
    std::atomic<bool>& m_needToDelayHandshake;
    utils::TimerManager::TimerGuard m_timerGuard;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class CustomHandshakeConnectionAcceptor:
    public ::testing::Test
{
public:
    using Acceptor = nx::network::CustomHandshakeConnectionAcceptor<
        AbstractStreamServerSocket,
        TestSocket>;

    ~CustomHandshakeConnectionAcceptor()
    {
        if (m_acceptor)
            m_acceptor->pleaseStopSync();
        if (m_connectionsGenerator)
            m_connectionsGenerator->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        auto tcpServer = std::make_unique<TCPServerSocket>(AF_INET);
        ASSERT_TRUE(tcpServer->bind(SocketAddress::anyPrivateAddressV4));
        ASSERT_TRUE(tcpServer->listen());
        ASSERT_TRUE(tcpServer->setNonBlockingMode(true));
        m_tcpServer = tcpServer.get();

        initializeAcceptor(std::move(tcpServer));
    }

    Acceptor& acceptor()
    {
        return *m_acceptor;
    }

    void givenManySilentConnectionCountGreaterThanListenQueue()
    {
        openSilentConnections(
            m_acceptor->readyConnectionQueueSize()*2);
    }

    void whenEstablishValidConnection()
    {
        m_clientConnection = std::make_unique<ssl::ClientStreamSocket>(
            ssl::Context::instance(),
            std::make_unique<TCPSocket>(AF_INET),
            ssl::kAcceptAnyCertificateCallback);

        ASSERT_TRUE(m_clientConnection->connect(
            m_tcpServer->getLocalAddress(),
            kNoTimeout));
    }

    void whenEstablishSilentConnection()
    {
        openSilentConnections(1);
        m_clientConnection = std::move(m_silentConnections.front());
        ASSERT_TRUE(m_clientConnection->setNonBlockingMode(false));
        m_silentConnections.pop_front();
    }

    void whenRawServerConnectionReportsError()
    {
        m_acceptor->pleaseStopSync();
        m_acceptor.reset();

        initializeAcceptor(
            std::make_unique<FailingTcpServerSocket>(SystemError::ioError));
    }

    void whenRawServerConnectionReportsTimedOut()
    {
        m_acceptor->pleaseStopSync();
        m_acceptor.reset();

        initializeAcceptor(
            std::make_unique<FailingTcpServerSocket>(SystemError::timedOut));
    }

    void thenConnectionIsAccepted()
    {
        ASSERT_NE(nullptr, m_acceptedConnections.pop().connection);
    }

    void thenConnectionIsClosedByServerAfterSomeTime()
    {
        std::array<char, 128> readBuf;
        ASSERT_EQ(0, m_clientConnection->recv(readBuf.data(), readBuf.size()))
            << SystemError::getLastOSErrorText();
    }

    void thenNoConnectionsHaveBeenAccepted()
    {
        ASSERT_TRUE(m_acceptedConnections.empty());
    }

    void thenCorrespondingErrorIsReported()
    {
        ASSERT_NE(SystemError::noError, m_acceptedConnections.pop().resultCode);
    }

    void thenErrorIsNotReported()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_TRUE(m_acceptedConnections.empty());
    }

    void startMultipleConnectAttempts()
    {
        static constexpr int kMaxSimultaneousConnections = 11;
        static constexpr int kBytesToSendThroughConnection = 1;

        m_connectionsGenerator = std::make_unique<ConnectionsGenerator>(
            m_tcpServer->getLocalAddress(),
            kMaxSimultaneousConnections,
            TestTrafficLimitType::incoming,
            kBytesToSendThroughConnection,
            ConnectionsGenerator::kInfiniteConnectionCount,
            TestTransmissionMode::spam);
        m_connectionsGenerator->setSocketFactory(
            []()
            {
                return std::make_unique<ssl::ClientStreamSocket>(
                    ssl::Context::instance(),
                    std::make_unique<TCPSocket>(AF_INET),
                    ssl::kAcceptAnyCertificateCallback);
            });
        m_connectionsGenerator->start();
    }

    void deleteAcceptorInItsAioThread()
    {
        m_acceptor->executeInAioThreadSync([this]() { m_acceptor.reset(); });
    }

    void stopAndDeleteAcceptor()
    {
        m_acceptor->pleaseStopSync();
        m_acceptor.reset();
    }

    void waitTillAllAioThreadsWereUsedForHandshake()
    {
        const auto allThreads = SocketGlobals::aioService().getAllAioThreads();
        std::vector<std::tuple<aio::AbstractAioThread*, bool /*used*/>> threadsWithUsageFlag;
        std::transform(
            allThreads.begin(), allThreads.end(),
            std::back_inserter(threadsWithUsageFlag),
            [](auto* aioThread) { return std::make_tuple(aioThread, false); });

        while (std::any_of(
            threadsWithUsageFlag.begin(), threadsWithUsageFlag.end(),
            [](const auto& val) { return !std::get<1>(val); }))
        {
            auto acceptResult = m_acceptedConnections.pop();
            if (acceptResult.connection)
            {
                ASSERT_EQ(
                    acceptResult.connection->getAioThread(),
                    acceptResult.connection->handshakeThread());

                std::get<1>(*std::find_if(
                    threadsWithUsageFlag.begin(), threadsWithUsageFlag.end(),
                    [aioThread = acceptResult.connection->handshakeThread()](
                        const auto& val)
                    {
                        return std::get<0>(val) == aioThread;
                    })) = true;
            }
        }
    }

    void clearSavedConnections()
    {
        m_acceptedConnections.clear();
    }

    void thenAllConnectionsAreDestroyed()
    {
        ASSERT_EQ(0, m_totalConnectionsCount);
    }

    void enableDelayingHandshakes()
    {
        m_needToDelayHandshake = true;
    }

private:
    struct AcceptResult
    {
        SystemError::ErrorCode resultCode;
        std::unique_ptr<TestSocket> connection;
    };

    std::unique_ptr<Acceptor> m_acceptor;
    nx::utils::SyncQueue<AcceptResult> m_acceptedConnections;
    TCPServerSocket* m_tcpServer = nullptr;
    std::unique_ptr<AbstractStreamSocket> m_clientConnection;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_silentConnections;
    std::unique_ptr<ConnectionsGenerator> m_connectionsGenerator;
    std::atomic<int> m_totalConnectionsCount = 0;
    nx::utils::TimerManager m_timerManager;
    std::atomic<bool> m_needToDelayHandshake = false;

    void initializeAcceptor(
        std::unique_ptr<AbstractStreamServerSocket> rawConnectionSocket)
    {
        using namespace std::placeholders;

        m_acceptor = std::make_unique<Acceptor>(
            std::move(rawConnectionSocket),
            [this](std::unique_ptr<AbstractStreamSocket> streamSocket)
            {
                return std::make_unique<TestSocket>(
                    ssl::Context::instance(),
                    std::move(streamSocket),
                    m_totalConnectionsCount,
                    m_timerManager,
                    m_needToDelayHandshake);
            });
        // Using very large timeout by default to make test reliable to various delays.
        m_acceptor->setHandshakeTimeout(std::chrono::hours(1));

        m_acceptor->start();
        m_acceptor->acceptAsync(
            std::bind(&CustomHandshakeConnectionAcceptor::saveAcceptedConnection, this, _1, _2));
    }

    void saveAcceptedConnection(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
    {
        using namespace std::placeholders;

        m_acceptedConnections.push(AcceptResult{
            systemErrorCode,
            nx::utils::static_unique_ptr_cast<TestSocket>(std::move(streamSocket))});

        m_acceptor->acceptAsync(
            std::bind(&CustomHandshakeConnectionAcceptor::saveAcceptedConnection, this, _1, _2));
    }

    void openSilentConnections(int count)
    {
        m_silentConnections.resize(count);
        nx::utils::SyncQueue<SystemError::ErrorCode> connectResults;

        for (int i = 0; i < count; ++i)
        {
            m_silentConnections[i] = std::make_unique<TCPSocket>(AF_INET);
            ASSERT_TRUE(m_silentConnections[i]->setNonBlockingMode(true));
            m_silentConnections[i]->connectAsync(
                m_tcpServer->getLocalAddress(),
                [&connectResults](SystemError::ErrorCode resultCode)
                {
                    connectResults.push(resultCode);
                });
        }

        for (int i = 0; i < count; ++i)
        {
            ASSERT_EQ(SystemError::noError, connectResults.pop());
        }
    }
};

TEST_F(CustomHandshakeConnectionAcceptor, connections_are_accepted)
{
    whenEstablishValidConnection();
    thenConnectionIsAccepted();
}

TEST_F(CustomHandshakeConnectionAcceptor, underlying_socket_accept_error_is_forwarded)
{
    whenRawServerConnectionReportsError();
    thenCorrespondingErrorIsReported();
}

TEST_F(CustomHandshakeConnectionAcceptor, underlying_socket_accept_timeout_is_not_forwarded)
{
    whenRawServerConnectionReportsTimedOut();
    thenErrorIsNotReported();
}

TEST_F(CustomHandshakeConnectionAcceptor, silent_connection_is_closed_by_server)
{
    acceptor().setHandshakeTimeout(std::chrono::milliseconds(1));

    whenEstablishSilentConnection();

    thenConnectionIsClosedByServerAfterSomeTime();
    thenNoConnectionsHaveBeenAccepted();
}

TEST_F(CustomHandshakeConnectionAcceptor, silent_connections_do_not_block_active_connections)
{
    givenManySilentConnectionCountGreaterThanListenQueue();
    whenEstablishValidConnection();
    thenConnectionIsAccepted();
}

TEST_F(
    CustomHandshakeConnectionAcceptor,
    different_connections_handshake_is_scaled_over_multiple_threads)
{
    // NOTE: this test makes sense only with multiple AIO threads.
    if (SocketGlobals::aioService().getAllAioThreads().size() <= 1)
        return;

    startMultipleConnectAttempts();
    waitTillAllAioThreadsWereUsedForHandshake();
}

// TODO: #akolesnikov Delete or enable in NXLIB-338.
TEST_F(CustomHandshakeConnectionAcceptor, DISABLED_acceptor_can_be_safely_deleted_in_the_middle_of_work)
{
    startMultipleConnectAttempts();

    std::this_thread::sleep_for(std::chrono::milliseconds(utils::random::number<int>(10, 300)));
    deleteAcceptorInItsAioThread();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    // The process does not crash.
}

TEST_F(CustomHandshakeConnectionAcceptor, acceptor_closes_all_connections_on_destruction)
{
    enableDelayingHandshakes();
    startMultipleConnectAttempts();

    std::this_thread::sleep_for(std::chrono::milliseconds(utils::random::number<int>(10, 300)));
    stopAndDeleteAcceptor();
    clearSavedConnections();
    thenAllConnectionsAreDestroyed();
}

} // namespace nx::network::test
