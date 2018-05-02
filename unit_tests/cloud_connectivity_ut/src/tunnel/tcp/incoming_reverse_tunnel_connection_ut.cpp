#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tcp/incoming_reverse_tunnel_connection.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

class IncomingReverseTunnelConnection:
    public ::testing::Test
{
protected:
    void startProxyServerThatAlwaysReportsError()
    {
        ASSERT_TRUE(m_proxyServer.bindAndListen());
    }

    void createConnection()
    {
        m_connection = std::make_unique<tcp::IncomingReverseTunnelConnection>(
            QnUuid::createUuid().toSimpleByteArray(),
            QnUuid::createUuid().toSimpleByteArray(),
            m_proxyServer.serverAddress());
    }

    void startConnectionToProxy()
    {
        using namespace std::placeholders;

        if (!m_connection)
            createConnection();

        RetryPolicy retryPolicy;
        retryPolicy.maxRetryCount = 1;

        m_connection->start(
            retryPolicy,
            std::bind(&IncomingReverseTunnelConnection::onConnectFinished, this, _1));
    }

    void assertConnectFailed()
    {
        ASSERT_NE(SystemError::noError, m_connectFinished.get_future().get());
    }

    void registerAdditionalConnectHandler(
        tcp::IncomingReverseTunnelConnection::StartHandler additionalHandler)
    {
        m_additionalHandler = std::move(additionalHandler);
    }

    void deleteConnection()
    {
        m_connection.reset();
    }

private:
    utils::promise<SystemError::ErrorCode> m_connectFinished;
    std::unique_ptr<tcp::IncomingReverseTunnelConnection> m_connection;
    nx::network::http::TestHttpServer m_proxyServer;
    tcp::IncomingReverseTunnelConnection::StartHandler m_additionalHandler;

    void onConnectFinished(SystemError::ErrorCode sysErrorCode)
    {
        if (m_additionalHandler)
            m_additionalHandler(sysErrorCode);
        m_connectFinished.set_value(sysErrorCode);
    }
};

TEST_F(IncomingReverseTunnelConnection, freeing_in_handler)
{
    startProxyServerThatAlwaysReportsError();
    registerAdditionalConnectHandler(
        [this](SystemError::ErrorCode /*sysErrorCode*/) { deleteConnection(); });
    startConnectionToProxy();
    assertConnectFailed();
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
