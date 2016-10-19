#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tcp/direct_endpoint_tunnel.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

TEST(TcpTunnel, cancellation)
{
    TestHttpServer testServer;
    ASSERT_TRUE(testServer.bindAndListen());

    aio::Timer timer;

    for (int i = 0; i < 50; ++i)
    {
        DirectTcpEndpointTunnel tunnel(
            timer.getAioThread(),
            "sessionId",
            testServer.serverAddress(),
            nullptr);

        for (int j = 0; j < 3; ++j)
        {
            SocketAttributes socketAttributes;
            socketAttributes.aioThread = 
                nx::network::SocketGlobals::aioService().getRandomAioThread();
            tunnel.establishNewConnection(
                std::chrono::seconds(5),
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

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
