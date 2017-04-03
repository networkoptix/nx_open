#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_outgoing_tunnel_connection.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/cpp14.h>

#include "cloud_relay_fixture_base.h"
#include "client_to_relay_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

//-------------------------------------------------------------------------------------------------
// Test fixture.

class CloudRelayOutgoingTunnelConnection:
    public CloudRelayFixtureBase
{
public:
    CloudRelayOutgoingTunnelConnection():
        m_clientToRelayConnectionCounter(0),
        m_destroyTunnelConnectionOnConnectFailure(false),
        m_relayType(RelayType::happy),
        m_connectTimeout(std::chrono::milliseconds::zero())
    {
    }

    ~CloudRelayOutgoingTunnelConnection()
    {
        if (m_tunnelConnection)
            m_tunnelConnection->pleaseStopSync();
    }

protected:
    void givenHappyRelay()
    {
        m_relayType = RelayType::happy;
    }

    void givenUnhappyRelay()
    {
        m_relayType = RelayType::unhappy;
    }
    
    void givenSilentRelay()
    {
        m_relayType = RelayType::silent;
    }

    void whenRequestingConnection()
    {
        using namespace std::placeholders;

        if (!m_tunnelConnection)
            createConnection();

        m_tunnelConnection->establishNewConnection(
            m_connectTimeout,
            nx::network::SocketAttributes(),
            std::bind(&CloudRelayOutgoingTunnelConnection::onConnectDone, this, _1, _2, _3));
        m_connectDone.get_future().wait();
    }

    void whenRequestingConnectionWithTimeout()
    {
        m_connectTimeout = std::chrono::milliseconds(1);
        whenRequestingConnection();
    }

    void thenRequestToRelayHasBeenIssued()
    {
        ASSERT_GT(m_clientToRelayConnectionCounter.load(), 0);
        // TODO: Check that request has been made
    }

    void thenConnectionHasBeenProvided()
    {
        thenRequestToRelayHasBeenIssued();

        ASSERT_EQ(SystemError::noError, m_connectResult.sysErrorCode);
        ASSERT_NE(nullptr, m_connectResult.connection);
        ASSERT_TRUE(m_connectResult.stillValid);
    }

    void thenErrorHasBeenReported()
    {
        thenRequestToRelayHasBeenIssued();

        ASSERT_NE(SystemError::noError, m_connectResult.sysErrorCode);
        ASSERT_EQ(nullptr, m_connectResult.connection);
        ASSERT_FALSE(m_connectResult.stillValid);
    }

    void thenTimedOutHasBeenReported()
    {
        thenErrorHasBeenReported();
        ASSERT_EQ(SystemError::timedOut, m_connectResult.sysErrorCode);
    }

    void thenTunnelReportedClosure()
    {
        ASSERT_NE(SystemError::noError, m_tunnelClosed.get_future().get());
    }

    void thenTunnelDidNotReportClosure()
    {
        ASSERT_EQ(
            std::future_status::timeout,
            m_tunnelClosed.get_future().wait_for(std::chrono::milliseconds::zero()));
    }

    void enableDestroyingTunnelConnectionOnConnectFailure()
    {
        m_destroyTunnelConnectionOnConnectFailure = true;
    }

private:
    enum class RelayType
    {
        happy,
        unhappy,
        silent,
    };

    struct Result
    {
        SystemError::ErrorCode sysErrorCode;
        std::unique_ptr<AbstractStreamSocket> connection;
        bool stillValid;

        Result():
            sysErrorCode(SystemError::noError),
            stillValid(false)
        {
        }

        Result(
            SystemError::ErrorCode sysErrorCode,
            std::unique_ptr<AbstractStreamSocket> connection,
            bool stillValid)
            :
            sysErrorCode(sysErrorCode),
            connection(std::move(connection)),
            stillValid(stillValid)
        {
        }
    };

    std::unique_ptr<relay::OutgoingTunnelConnection> m_tunnelConnection;
    Result m_connectResult;
    nx::utils::promise<void> m_connectDone;
    SocketAddress m_relayEndpoint;
    std::atomic<int> m_clientToRelayConnectionCounter;
    bool m_isRelayHappy;
    nx::utils::promise<SystemError::ErrorCode> m_tunnelClosed;
    bool m_destroyTunnelConnectionOnConnectFailure;
    RelayType m_relayType;
    std::chrono::milliseconds m_connectTimeout;

    virtual void onClientToRelayConnectionInstanciated(
        ClientToRelayConnection* relayClient) override
    {
        switch (m_relayType)
        {
            case RelayType::unhappy:
                relayClient->setFailRequests(true);
                break;
            case RelayType::silent:
                relayClient->setIgnoreRequests(true);
                break;
            default:
                break;
        }

        ++m_clientToRelayConnectionCounter;
    }

    void createConnection()
    {
        using namespace std::placeholders;

        auto clientToRelayConnection =
            api::ClientToRelayConnectionFactory::create(m_relayEndpoint);

        m_tunnelConnection = std::make_unique<relay::OutgoingTunnelConnection>(
            m_relayEndpoint,
            nx::String(),
            std::move(clientToRelayConnection));
        m_tunnelConnection->setControlConnectionClosedHandler(
            std::bind(&CloudRelayOutgoingTunnelConnection::onTunnelClosed, this, _1));

        m_clientToRelayConnectionCounter = 0;
    }

    void onTunnelClosed(SystemError::ErrorCode sysErrorCode)
    {
        ASSERT_TRUE(m_tunnelConnection->isInSelfAioThread());
        m_tunnelClosed.set_value(sysErrorCode);
    }

    void onConnectDone(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection,
        bool stillValid)
    {
        ASSERT_TRUE(m_tunnelConnection->isInSelfAioThread());

        if (m_destroyTunnelConnectionOnConnectFailure)
            m_tunnelConnection.reset();

        m_connectResult = Result(sysErrorCode, std::move(connection), stillValid);
        m_connectDone.set_value();
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(CloudRelayOutgoingTunnelConnection, asks_relay_for_connection_using_proper_session_id)
{
    givenHappyRelay();
    whenRequestingConnection();
    thenConnectionHasBeenProvided();
}

TEST_F(CloudRelayOutgoingTunnelConnection, shuts_down_on_receiving_error_from_relay)
{
    givenUnhappyRelay();

    whenRequestingConnection();

    thenErrorHasBeenReported();
    thenTunnelReportedClosure();
}

TEST_F(CloudRelayOutgoingTunnelConnection, tunnel_removed_from_closed_handler)
{
    givenUnhappyRelay();
    enableDestroyingTunnelConnectionOnConnectFailure();

    whenRequestingConnection();

    thenErrorHasBeenReported();
    thenTunnelDidNotReportClosure();
}

TEST_F(CloudRelayOutgoingTunnelConnection, connect_timeout)
{
    givenSilentRelay();

    whenRequestingConnectionWithTimeout();

    thenTimedOutHasBeenReported();
    thenTunnelReportedClosure();
}

//TEST_F(CloudRelayOutgoingTunnelConnection, reopens_control_connection_on_failure)
//TEST_F(CloudRelayOutgoingTunnelConnection, shuts_down_on_control_connection_failure)
//TEST_F(CloudRelayOutgoingTunnelConnection, multiple_concurrent_connections)
//TEST_F(CloudRelayOutgoingTunnelConnection, inactivity_timeout)
//TEST_F(CloudRelayOutgoingTunnelConnection, inactivity_timeout_does_not_start_while_provided_connections_are_in_use)
//TEST_F(CloudRelayOutgoingTunnelConnection, provided_connection_destroyed_after_tunnel_connection)
//TEST_F(CloudRelayOutgoingTunnelConnection, terminating)

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
