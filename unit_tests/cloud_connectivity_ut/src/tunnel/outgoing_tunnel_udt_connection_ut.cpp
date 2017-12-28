#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/cloud/tunnel/udp/outgoing_tunnel_connection.h>
#include <nx/network/socket_global.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/scope_guard.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {

class OutgoingTunnelConnectionTest
:
    public ::testing::Test
{
public:
    OutgoingTunnelConnectionTest()
    :
        m_serverSocket(std::make_unique<UdtStreamServerSocket>(AF_INET)),
        m_first(true)
    {
    }

    ~OutgoingTunnelConnectionTest()
    {
        if (m_serverSocket)
            m_serverSocket->pleaseStopSync();
        if (m_controlConnection)
            m_controlConnection->pleaseStopSync();
    }

    bool start()
    {
        if (!m_serverSocket->bind(SocketAddress(HostAddress::localhost, 0)) ||
            !m_serverSocket->listen() ||
            !m_serverSocket->setNonBlockingMode(true))
        {
            return false;
        }
        m_serverSocket->acceptAsync(
            std::bind(
                &OutgoingTunnelConnectionTest::onNewConnectionAccepted,
                this,
                std::placeholders::_1,
                std::placeholders::_2));
        //udt has a feature: it can eat connection that has been established just after listen call
            //client side will think it has connected, but server side knows nothing abount it
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        return true;
    }

    SocketAddress serverEndpoint() const
    {
        return m_serverSocket->getLocalAddress();
    }

protected:
    struct ConnectResult
    {
        SystemError::ErrorCode errorCode;
        std::unique_ptr<AbstractStreamSocket> connection;
        bool stillValid;
    };

    struct ConnectContext
    {
        nx::utils::promise<ConnectResult> connectedPromise;
        std::chrono::milliseconds timeout;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
    };

    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    std::unique_ptr<AbstractStreamSocket> m_controlConnection;
    std::list<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;

    std::vector<ConnectContext> startConnections(
        udp::OutgoingTunnelConnection* const tunnelConnection,
        size_t connectionsToCreate,
        boost::optional<int> minTimeoutMillis,
        boost::optional<int> maxTimeoutMillis)
    {
        std::vector<ConnectContext> connections(connectionsToCreate);
        for (auto& connectContext : connections)
        {
            if (minTimeoutMillis && maxTimeoutMillis)
            {
                connectContext.timeout = std::chrono::milliseconds(
                    nx::utils::random::number(*minTimeoutMillis, *maxTimeoutMillis));
                connectContext.startTime = std::chrono::steady_clock::now();
            }

            tunnelConnection->establishNewConnection(
                connectContext.timeout,
                SocketAttributes(),
                [&connectContext](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> connection,
                    bool stillValid)
            {
                connectContext.endTime = std::chrono::steady_clock::now();
                connectContext.connectedPromise.set_value(
                    ConnectResult{ errorCode, std::move(connection), stillValid });
            });
        }
        return connections;
    }

    std::vector<ConnectContext> startConnections(
        OutgoingTunnelConnection* const tunnelConnection,
        size_t connectionsToCreate)
    {
        return startConnections(
            tunnelConnection,
            connectionsToCreate,
            boost::none,
            boost::none);
    }

    void setControlConnectionEstablishedHander(
        nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_onControlConnectionEstablishedHander = std::move(handler);
    }

private:
    bool m_first;
    nx::utils::MoveOnlyFunc<void()> m_onControlConnectionEstablishedHander;

    void onNewConnectionAccepted(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractStreamSocket> socket)
    {
        using namespace std::placeholders;

        if (m_first)
        {
            NX_ASSERT(errorCode == SystemError::noError);
            m_controlConnection = std::move(socket);
            m_first = false;
            if (m_onControlConnectionEstablishedHander)
                m_onControlConnectionEstablishedHander();
        }
        else
        {
            if (errorCode == SystemError::noError)
            {
                m_acceptedSockets.emplace_back(std::move(socket));
                //TODO #ak waiting for accepted socket to close
            }
        }

        m_serverSocket->acceptAsync(
            std::bind(
                &OutgoingTunnelConnectionTest::onNewConnectionAccepted, this,
                _1, _2));
    }
};

TEST_F(OutgoingTunnelConnectionTest, common)
{
    const int connectionsToCreate = 100;

    ASSERT_TRUE(start()) << SystemError::getLastOSErrorText().toStdString();

    auto udtConnection = std::make_unique<UdtStreamSocket>(AF_INET);
    ASSERT_TRUE(udtConnection->connect(serverEndpoint(), nx::network::kNoTimeout));
    const auto localAddress = udtConnection->getLocalAddress();

    OutgoingTunnelConnection tunnelConnection(
        nx::network::SocketGlobals::aioService().getRandomAioThread(),
        QnUuid::createUuid().toByteArray(),
        std::move(udtConnection));
    tunnelConnection.start();

    auto connectContexts = startConnections(&tunnelConnection, connectionsToCreate);

    //waiting for all connections to complete
    std::list<ConnectResult> connectResults;
    for (auto& connectContext : connectContexts)
        connectResults.emplace_back(connectContext.connectedPromise.get_future().get());

    //checking that connection uses right port to connect
    for (const auto& sock : m_acceptedSockets)
    {
        ASSERT_EQ(localAddress, sock->getForeignAddress());
    }

    for (auto& result: connectResults)
    {
        ASSERT_EQ(SystemError::noError, result.errorCode);
        ASSERT_NE(nullptr, result.connection);
        ASSERT_TRUE(result.stillValid);
    }

    tunnelConnection.pleaseStopSync();
}

TEST_F(OutgoingTunnelConnectionTest, timeout)
{
    const int connectionsToCreate = 100;
    const int minTimeoutMillis = 50;
    const int maxTimeoutMillis = 2000;
    //TODO #ak fault is too large. Investigate!
    const std::chrono::milliseconds acceptableTimeoutFault(500);

    ASSERT_TRUE(start()) << SystemError::getLastOSErrorText().toStdString();

    auto udtConnection = std::make_unique<UdtStreamSocket>(AF_INET);
    ASSERT_TRUE(udtConnection->connect(serverEndpoint(), nx::network::kNoTimeout));

    OutgoingTunnelConnection tunnelConnection(
        nx::network::SocketGlobals::aioService().getRandomAioThread(),
        QnUuid::createUuid().toByteArray(),
        std::move(udtConnection));
    auto tunnelConnectionGuard = makeScopeGuard(
        [&tunnelConnection]() { tunnelConnection.pleaseStopSync(); });

    tunnelConnection.start();

    m_serverSocket->pleaseStopSync();
    m_serverSocket.reset();
    //server socket is stopped, no connection can be accepted

    auto connectContexts =
        startConnections(
            &tunnelConnection,
            connectionsToCreate,
            minTimeoutMillis,
            maxTimeoutMillis);

    for (std::size_t i = 0; i < connectContexts.size(); ++i)
    {
        const auto result = connectContexts[i].connectedPromise.get_future().get();
        ASSERT_EQ(SystemError::timedOut, result.errorCode);
        ASSERT_EQ(nullptr, result.connection);
        ASSERT_TRUE(result.stillValid);

        #ifdef _DEBUG
            if (!utils::TestOptions::areTimeAssertsDisabled())
            {
                const auto connectTime =
                    connectContexts[i].endTime - connectContexts[i].startTime;

                auto connectTimeDiff =
                    connectTime > connectContexts[i].timeout
                    ? connectTime - connectContexts[i].timeout
                    : connectContexts[i].timeout - connectTime;

                ASSERT_LE(connectTimeDiff, acceptableTimeoutFault);
            }
        #endif
    }
}

TEST_F(OutgoingTunnelConnectionTest, cancellation)
{
    const int loopLength = 10;
    const int connectionsToCreate = 100;

    ASSERT_TRUE(start()) << SystemError::getLastOSErrorText().toStdString();

    for (int i = 0; i < loopLength; ++i)
    {
        auto udtConnection = std::make_unique<UdtStreamSocket>(AF_INET);
        ASSERT_TRUE(udtConnection->connect(serverEndpoint(), nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText().toStdString();

        OutgoingTunnelConnection tunnelConnection(
            nx::network::SocketGlobals::aioService().getRandomAioThread(),
            QnUuid::createUuid().toByteArray(),
            std::move(udtConnection));
        tunnelConnection.start();

        auto connectContexts = startConnections(&tunnelConnection, connectionsToCreate);

        tunnelConnection.pleaseStopSync();

        for (auto& connectContext : connectContexts)
        {
            auto future = connectContext.connectedPromise.get_future();
            ASSERT_TRUE(future.valid());
            if (future.wait_for(std::chrono::seconds::zero()) == std::future_status::ready)
            {
                auto result = future.get();
                ASSERT_TRUE(
                    result.errorCode == SystemError::noError ||
                    result.errorCode == SystemError::interrupted);
            }
        }
    }
}

TEST_F(OutgoingTunnelConnectionTest, controlConnectionFailure)
{
    const std::chrono::seconds controlConnectionEstablishedTimeout(1);

    ASSERT_TRUE(start()) << SystemError::getLastOSErrorText().toStdString();

    nx::utils::promise<void> controlConnectionEstablishedPromise;
    setControlConnectionEstablishedHander(
        [&controlConnectionEstablishedPromise]
        {
            controlConnectionEstablishedPromise.set_value();
        });

    const auto serverAddress = serverEndpoint();
    auto udtConnection = std::make_unique<UdtStreamSocket>(AF_INET);
    ASSERT_TRUE(udtConnection->connect(serverAddress, nx::network::kNoTimeout))
        << SystemError::getLastOSErrorText().toStdString();

    Timeouts udpTunnelKeepAlive;
    udpTunnelKeepAlive.keepAlivePeriod = std::chrono::seconds(1);
    OutgoingTunnelConnection tunnelConnection(
        nx::network::SocketGlobals::aioService().getRandomAioThread(),
        QnUuid::createUuid().toByteArray(),
        std::move(udtConnection),
        udpTunnelKeepAlive);
    tunnelConnection.start();

    nx::utils::promise<void> controlConnectionClosedPromise;
    tunnelConnection.setControlConnectionClosedHandler(
        [&controlConnectionClosedPromise](SystemError::ErrorCode /*errorCode*/)
        {
            controlConnectionClosedPromise.set_value();
        });

    auto tunnelConnectionGuard = makeScopeGuard(
        [&tunnelConnection]{ tunnelConnection.pleaseStopSync(); });
    auto controlConnectionGuard = makeScopeGuard(
        [this]
        {
            if (!m_controlConnection)
                return;
            m_controlConnection->pleaseStopSync();
            m_controlConnection.reset();
        });

    ASSERT_EQ(
        std::future_status::ready,
        controlConnectionEstablishedPromise.get_future().wait_for(
            controlConnectionEstablishedTimeout));

    ASSERT_NE(nullptr, m_controlConnection);
    controlConnectionGuard.fire();

    //waiting for control connection to close
    const auto t1 = std::chrono::steady_clock::now();
    ASSERT_EQ(
        std::future_status::ready,
        controlConnectionClosedPromise.get_future().wait_for(
            udpTunnelKeepAlive.maxConnectionInactivityPeriod() * 10));

    #ifdef _DEBUG
        if (!utils::TestOptions::areTimeAssertsDisabled())
        {
            EXPECT_LT(
                std::chrono::steady_clock::now() - t1,
                udpTunnelKeepAlive.maxConnectionInactivityPeriod() * 15 / 10);
        }
    #else
        static_cast<void>(t1);
    #endif

    auto connectContexts = startConnections(&tunnelConnection, 1);
    auto future = connectContexts[0].connectedPromise.get_future();
    auto result = future.get();
    ASSERT_FALSE(result.stillValid);    //tunnel is invalid since control connection has timed out
    ASSERT_NE(nullptr, result.connection);  //but connection still succeeded
    ASSERT_EQ(SystemError::noError, result.errorCode);
}

}   //namespace udp
}   //namespace cloud
}   //namespace network
}   //namespace nx
