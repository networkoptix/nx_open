#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tcp/direct_endpoint_tunnel.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/utils/string.h>
#include <nx/utils/std/future.h>

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
    constexpr static auto defaultConnectTimeout = std::chrono::seconds(10);

    TcpTunnel()
    {
        init();
    }

    ~TcpTunnel()
    {
        if (m_tunnel)
            m_tunnel->pleaseStopSync();
    }

    void givenOpenedTcpTunnelToRemotePeer()
    {
        m_tunnel = std::make_unique<DirectTcpEndpointTunnel>(
            nx::network::SocketGlobals::aioService().getRandomAioThread(),
            nx::utils::generateRandomName(7),
            m_testServer->serverAddress(),
            nullptr);

        expectingTunnelFunctionsSuccessfully();
    }
    
    void whenRemotePeerHasDisappeared()
    {
        m_testServer.reset();
    }
    
    void expectingTunnelFunctionsSuccessfully()
    {
        openNewConnection(
            [](
                SystemError::ErrorCode resultCode,
                std::unique_ptr<AbstractStreamSocket> socket,
                bool stillValid)
            {
                ASSERT_EQ(SystemError::noError, resultCode);
                ASSERT_NE(nullptr, socket);
                ASSERT_TRUE(stillValid);
            });
    }

    void expectingTunnelReportsErrorOnConnectRequest()
    {
        openNewConnection(
            [](
                SystemError::ErrorCode resultCode,
                std::unique_ptr<AbstractStreamSocket> socket,
                bool stillValid)
            {
                ASSERT_NE(SystemError::noError, resultCode);
                ASSERT_EQ(nullptr, socket);
                ASSERT_FALSE(stillValid);
            });
    }

protected:
    TestHttpServer& testServer()
    {
        return *m_testServer;
    }

private:
    std::unique_ptr<TestHttpServer> m_testServer;
    std::unique_ptr<DirectTcpEndpointTunnel> m_tunnel;

    void init()
    {
        m_testServer = std::make_unique<TestHttpServer>();
        ASSERT_TRUE(m_testServer->bindAndListen());
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
                std::unique_ptr<AbstractStreamSocket> socket,
                bool stillValid)
            {
                onConnectedHandler(resultCode, std::move(socket), stillValid);
                connectedPromise.set_value();
            });
        connectedPromise.get_future().wait();
    }
};

TEST_F(TcpTunnel, cancellation)
{
    aio::Timer timer;

    for (int i = 0; i < 50; ++i)
    {
        DirectTcpEndpointTunnel tunnel(
            timer.getAioThread(),
            "sessionId",
            testServer().serverAddress(),
            nullptr);

        for (int j = 0; j < 3; ++j)
        {
            SocketAttributes socketAttributes;
            socketAttributes.aioThread = 
                nx::network::SocketGlobals::aioService().getRandomAioThread();
            tunnel.establishNewConnection(
                defaultConnectTimeout,
                std::move(socketAttributes),
                [](SystemError::ErrorCode /*sysErrorCode*/,
                   std::unique_ptr<AbstractStreamSocket>,
                   bool /*stillValid*/)
                {
                });
        }
        tunnel.pleaseStopSync();
    }
}

TEST_F(TcpTunnel, target_peer_has_disappeared)
{
    givenOpenedTcpTunnelToRemotePeer();
    whenRemotePeerHasDisappeared();
    expectingTunnelReportsErrorOnConnectRequest();
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
