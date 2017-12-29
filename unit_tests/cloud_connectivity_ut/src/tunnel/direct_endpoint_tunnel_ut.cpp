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
    TcpTunnel()
    {
        init();
    }

    ~TcpTunnel()
    {
        if (m_tunnel)
            m_tunnel->pleaseStopSync();
    }

    std::unique_ptr<DirectTcpEndpointTunnel> makeTunnel(
        aio::AbstractAioThread* aioThread, String sessionId)
    {
        auto socket = std::make_unique<TCPSocket>(AF_INET);
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
            nx::utils::generateRandomName(7));

        m_tunnel->setControlConnectionClosedHandler(
            [this](SystemError::ErrorCode code) { m_tunnelClosedPromise.set_value(code); });

        m_tunnel->start();
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
    std::unique_ptr<nx::network::http::TestHttpServer> m_testServer;
    std::unique_ptr<DirectTcpEndpointTunnel> m_tunnel;
    utils::promise<SystemError::ErrorCode> m_tunnelClosedPromise;

    void init()
    {
        m_testServer = std::make_unique<nx::network::http::TestHttpServer>();
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
                std::unique_ptr<nx::network::AbstractStreamSocket> socket,
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

TEST_F(TcpTunnel, target_peer_has_disappeared)
{
    givenOpenedTcpTunnelToRemotePeer();
    whenRemotePeerHasDisappeared();
    expectingTunnelClosure(SystemError::connectionReset);
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
