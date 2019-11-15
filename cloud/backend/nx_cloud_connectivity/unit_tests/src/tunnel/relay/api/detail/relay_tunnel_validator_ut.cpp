#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/detail/relay_tunnel_validator.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_notifications.h>
#include <nx/network/system_socket.h>

#include <nx/utils/thread/sync_queue.h>

namespace nx::cloud::relay::api::detail::test {

class RelayTunnelValidator:
    public ::testing::Test
{
    using base_type = ::testing::Test;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_server = std::make_unique<nx::network::TCPServerSocket>(AF_INET);
        ASSERT_TRUE(m_server->bind(nx::network::SocketAddress::anyPrivateAddress)
            && m_server->listen()) << SystemError::getLastOSErrorText().toStdString();
    }

    void givenValidator()
    {
        establishConnection();
        prepareOpenTunnelResponse();

        ASSERT_TRUE(m_clientConnection->setNonBlockingMode(true));

        m_validator = std::make_unique<detail::TunnelValidator>(
            std::exchange(m_clientConnection, nullptr),
            m_openTunnelResponse);

        m_validator->validate(
            [this](auto result) { saveValidationResult(result); });
    }

    void whenRelayKeepAliveIsSentThroughTheConnection()
    {
        const auto buffer = KeepAliveNotification().toHttpMessage().toString() + "\r\n\r\n";
        ASSERT_EQ(
            buffer.size(),
            m_serverConnection->send(buffer.data(), buffer.size()));
    }

    void whenRelayOpenTunnelIsSentThroughTheConnection()
    {
        const auto buffer = OpenTunnelNotification().toHttpMessage().toString() + "\r\n\r\n";
        ASSERT_EQ(
            buffer.size(),
            m_serverConnection->send(buffer.data(), buffer.size()));
    }

    void whenHttpResponseIsSentThroughTheConnection()
    {
        // Any response will do, so using what we have already.
        const auto buffer = m_openTunnelResponse.toString() + "\r\n\r\n";
        ASSERT_EQ(
            buffer.size(),
            m_serverConnection->send(buffer.data(), buffer.size()));
    }

    void thenValidationSucceeded()
    {
        ASSERT_EQ(nx::network::http::tunneling::ResultCode::ok, m_validationResults.pop());
    }

    void thenValidationFailed()
    {
        ASSERT_NE(nx::network::http::tunneling::ResultCode::ok, m_validationResults.pop());
    }

private:
    std::unique_ptr<nx::network::TCPServerSocket> m_server;
    std::unique_ptr<nx::network::TCPSocket> m_clientConnection;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_serverConnection;
    std::unique_ptr<detail::TunnelValidator> m_validator;
    nx::network::http::Response m_openTunnelResponse;
    nx::utils::SyncQueue<nx::network::http::tunneling::ResultCode> m_validationResults;

    void establishConnection()
    {
        m_clientConnection = std::make_unique<nx::network::TCPSocket>(AF_INET);

        ASSERT_TRUE(m_clientConnection->connect(
            m_server->getLocalAddress(), nx::network::kNoTimeout));

        m_serverConnection = m_server->accept();
        ASSERT_NE(nullptr, m_serverConnection);
    }

    void prepareOpenTunnelResponse()
    {
        m_openTunnelResponse.statusLine.statusCode = nx::network::http::StatusCode::ok;
        m_openTunnelResponse.statusLine.version = nx::network::http::http_1_1;
        m_openTunnelResponse.headers.emplace(
            nx::cloud::relay::api::kNxProtocolHeader, nx::cloud::relay::api::kRelayProtocol);
    }

    void saveValidationResult(nx::network::http::tunneling::ResultCode resultCode)
    {
        m_validationResults.push(resultCode);
    }
};

TEST_F(RelayTunnelValidator, relay_keep_alive_notification_validates_the_tunnel)
{
    givenValidator();
    whenRelayKeepAliveIsSentThroughTheConnection();
    thenValidationSucceeded();
}

TEST_F(RelayTunnelValidator, validation_is_failed_on_receiving_open_tunnel_notification)
{
    givenValidator();
    whenRelayOpenTunnelIsSentThroughTheConnection();
    thenValidationFailed();
}

// Use-case: in case of GET/POST tunnel, a smart ass firewall can send a response to POST.
TEST_F(RelayTunnelValidator, unexpected_response_fails_validation)
{
    givenValidator();
    whenHttpResponseIsSentThroughTheConnection();
    thenValidationFailed();
}

} // namespace nx::cloud::relay::api::detail::test
