#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/aio/timer.h>
#include <nx/network/cloud/tunnel/tcp/direct_endpoint_tunnel.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

namespace {

constexpr auto defaultConnectTimeout = std::chrono::seconds(10);

} // namespace

class TcpTunnel:
    public ::testing::Test
{
public:
    ~TcpTunnel()
    {
        if (m_tunnel)
            m_tunnel->pleaseStopSync();
    }

    std::unique_ptr<DirectTcpEndpointTunnel> makeTunnel(
        aio::AbstractAioThread* aioThread, std::string sessionId)
    {
        const auto ipVersion = m_testServer->serverAddress().address.isPureIpV6()
            ? AF_INET6 : AF_INET;

        auto socket = std::make_unique<TCPSocket>(ipVersion);
        if (!socket->connect(m_testServer->serverAddress(), nx::network::deprecated::kDefaultConnectTimeout) ||
            !socket->setNonBlockingMode(true))
        {
            return nullptr;
        }

        return std::make_unique<DirectTcpEndpointTunnel>(
            aioThread, sessionId, m_testServer->serverAddress(), std::move(socket));
    }

    void givenOpenedTcpTunnelToRemotePeer()
    {
        m_tunnel = makeTunnel(
            nx::network::SocketGlobals::aioService().getRandomAioThread(),
            nx::utils::generateRandomName(7).toStdString());

        m_tunnel->setControlConnectionClosedHandler(
            [this](SystemError::ErrorCode code) { m_tunnelClosedPromise.set_value(code); });

        m_tunnel->start();
    }

    void givenOpenedTcpTunnelToRemotePeerOverIpv6()
    {
        startIpv6Server();

        givenOpenedTcpTunnelToRemotePeer();
    }

    void whenRequestNewConnection()
    {
        m_tunnel->establishNewConnection(
            kNoTimeout,
            SocketAttributes(),
            [this](auto&&... args) { saveConnectResult(std::forward<decltype(args)>(args)...); });
    }

    void whenRemotePeerHasDisappeared()
    {
        m_testServer.reset();
    }

    void thenConnectionIsEstablished()
    {
        m_lastConnectResult = m_connectResults.pop();

        ASSERT_EQ(SystemError::noError, m_lastConnectResult.resultCode);
        ASSERT_NE(nullptr, m_lastConnectResult.connection);
        ASSERT_TRUE(m_lastConnectResult.stillValid);
    }

    void assertTunnelWorks()
    {
        openNewConnection(
            [](
                SystemError::ErrorCode resultCode,
                std::unique_ptr<nx::network::AbstractStreamSocket> socket,
                bool stillValid)
            {
                ASSERT_EQ(SystemError::noError, resultCode);
                ASSERT_NE(nullptr, socket);
                ASSERT_TRUE(stillValid);
            });
    }

    void expectingTunnelClosure(SystemError::ErrorCode code)
    {
        EXPECT_EQ(code, m_tunnelClosedPromise.get_future().get());
    }

protected:
    nx::network::http::TestHttpServer& testServer()
    {
        return *m_testServer;
    }

private:
    struct ConnectResult
    {
        SystemError::ErrorCode resultCode = SystemError::noError;
        std::unique_ptr<AbstractStreamSocket> connection;
        bool stillValid = false;
    };

    std::unique_ptr<nx::network::http::TestHttpServer> m_testServer;
    std::unique_ptr<DirectTcpEndpointTunnel> m_tunnel;
    utils::promise<SystemError::ErrorCode> m_tunnelClosedPromise;
    nx::utils::SyncQueue<ConnectResult> m_connectResults;
    ConnectResult m_lastConnectResult;

    virtual void SetUp() override
    {
        startIpv4Server();
    }

    void startIpv4Server()
    {
        m_testServer = std::make_unique<nx::network::http::TestHttpServer>();
        ASSERT_TRUE(m_testServer->bindAndListen());
    }

    void startIpv6Server()
    {
        m_testServer = std::make_unique<nx::network::http::TestHttpServer>(
            std::make_unique<TCPServerSocket>(AF_INET6));

        ASSERT_TRUE(m_testServer->bindAndListen(SocketAddress::anyPrivateAddressV6))
            << SystemError::getLastOSErrorText().toStdString();
    }

    template<typename OnConnectedHandler>
    void openNewConnection(OnConnectedHandler onConnectedHandler)
    {
        nx::utils::promise<void> connectedPromise;

        SocketAttributes socketAttributes;
        socketAttributes.sendTimeout = 1000;
        m_tunnel->establishNewConnection(
            defaultConnectTimeout,
            std::move(socketAttributes),
            [&connectedPromise, onConnectedHandler = std::move(onConnectedHandler)](
                SystemError::ErrorCode resultCode,
                std::unique_ptr<nx::network::AbstractStreamSocket> socket,
                bool stillValid)
            {
                onConnectedHandler(resultCode, std::move(socket), stillValid);
                connectedPromise.set_value();
            });
        connectedPromise.get_future().wait();
    }

    void saveConnectResult(
        SystemError::ErrorCode resultCode,
        std::unique_ptr<AbstractStreamSocket> connection,
        bool stillValid)
    {
        m_connectResults.push({resultCode, std::move(connection), stillValid});
    }
};

TEST_F(TcpTunnel, cancellation)
{
    aio::Timer timer;

    for (int i = 0; i < 50; ++i)
    {
        const auto tunnel = makeTunnel(timer.getAioThread(), "sessionId");
        for (int j = 0; j < 3; ++j)
        {
            SocketAttributes socketAttributes;
            socketAttributes.aioThread =
                nx::network::SocketGlobals::aioService().getRandomAioThread();
            tunnel->establishNewConnection(
                defaultConnectTimeout,
                std::move(socketAttributes),
                [](SystemError::ErrorCode /*sysErrorCode*/,
                   std::unique_ptr<nx::network::AbstractStreamSocket>,
                   bool /*stillValid*/)
                {
                });
        }
        tunnel->pleaseStopSync();
    }
}

TEST_F(TcpTunnel, DISABLED_tunnel_is_broken_when_target_peer_disappears)
{
    givenOpenedTcpTunnelToRemotePeer();
    whenRemotePeerHasDisappeared();
    expectingTunnelClosure(SystemError::connectionReset);
}

TEST_F(TcpTunnel, connection_can_be_established)
{
    givenOpenedTcpTunnelToRemotePeer();
    whenRequestNewConnection();
    thenConnectionIsEstablished();
}

TEST_F(TcpTunnel, tunnel_over_ipv6_is_supported)
{
    givenOpenedTcpTunnelToRemotePeerOverIpv6();
    whenRequestNewConnection();
    thenConnectionIsEstablished();
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
