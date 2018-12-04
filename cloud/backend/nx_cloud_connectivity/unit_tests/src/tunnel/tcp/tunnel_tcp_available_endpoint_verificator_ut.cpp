#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_available_endpoint_verificator.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

class TunnelTcpAvailableEndpointVerificator:
    public ::testing::Test
{
public:
    TunnelTcpAvailableEndpointVerificator()
    {
        m_verificator = std::make_unique<AvailableEndpointVerificator>("some_session_id");
    }

    ~TunnelTcpAvailableEndpointVerificator()
    {
        if (m_verificator)
            m_verificator->pleaseStopSync();
    }

protected:
    void givenListeningEndpoint()
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());
        m_endpoint = m_httpServer.serverAddress();
    }

    void givenUnavailableEndpoint()
    {
        m_endpoint = nx::network::SocketAddress("example.com", 37);

        m_verificator->setTimeout(std::chrono::milliseconds(100));
    }

    void whenInvokedVerificator()
    {
        using namespace std::placeholders;

        m_verificator->verifyHost(
            m_endpoint,
            AddressEntry(),
            std::bind(&TunnelTcpAvailableEndpointVerificator::onVerificationDone, this, _1));
    }

    void thenSuccessHasBeenReported()
    {
        ASSERT_EQ(
            AbstractEndpointVerificator::VerificationResult::passed,
            m_verificationResults.pop());
        ASSERT_EQ(SystemError::noError, m_verificator->lastSystemErrorCode());
    }

    void thenErrorHasBeenReported()
    {
        ASSERT_EQ(
            AbstractEndpointVerificator::VerificationResult::ioError,
            m_verificationResults.pop());
        ASSERT_NE(SystemError::noError, m_verificator->lastSystemErrorCode());
    }

private:
    std::unique_ptr<AvailableEndpointVerificator> m_verificator;
    nx::network::http::TestHttpServer m_httpServer;
    nx::utils::SyncQueue<AbstractEndpointVerificator::VerificationResult> m_verificationResults;
    nx::network::SocketAddress m_endpoint;

    void onVerificationDone(
        AbstractEndpointVerificator::VerificationResult result)
    {
        m_verificationResults.push(result);
    }
};

TEST_F(TunnelTcpAvailableEndpointVerificator, verifies_available_endpoint)
{
    givenListeningEndpoint();
    whenInvokedVerificator();
    thenSuccessHasBeenReported();
}

TEST_F(TunnelTcpAvailableEndpointVerificator, reports_error_for_unavailable_endpoint)
{
    givenUnavailableEndpoint();
    whenInvokedVerificator();
    thenErrorHasBeenReported();
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
