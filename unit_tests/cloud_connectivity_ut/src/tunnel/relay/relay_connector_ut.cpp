#include <atomic>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connector.h>
#include <nx/utils/std/future.h>

#include "client_to_relay_connection.h"
#include "cloud_relay_fixture_base.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

//-------------------------------------------------------------------------------------------------
// Test fixture.

class CloudRelayConnector:
    public CloudRelayFixtureBase
{
public:
    CloudRelayConnector():
        m_prevClientToRelayConnectionInstanciated(nullptr)
    {
    }

    ~CloudRelayConnector()
    {
        m_connector->pleaseStopSync();
    }

protected:
    void givenHappyRelay()
    {
        m_relayEndpoint = SocketAddress(HostAddress::localhost, 12345);

        m_connector = std::make_unique<relay::Connector>(
            m_relayEndpoint,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
    }

    void givenUnhappyRelay()
    {
        m_relayEndpoint = SocketAddress(HostAddress::localhost, 12345);
        m_connector = std::make_unique<relay::Connector>(
            m_relayEndpoint,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
        m_prevClientToRelayConnectionInstanciated.load()->setFailRequests(true);
    }

    void givenSilentRelay()
    {
        m_relayEndpoint = SocketAddress(HostAddress::localhost, 12345);

        m_connector = std::make_unique<relay::Connector>(
            m_relayEndpoint,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
        m_prevClientToRelayConnectionInstanciated.load()->setIgnoreRequests(true);
    }

    void whenConnectorIsInvoked()
    {
        using namespace std::placeholders;

        m_connector->connect(
            hpm::api::ConnectResponse(),
            std::chrono::milliseconds::zero(),
            std::bind(&CloudRelayConnector::onConnectFinished, this, _1, _2, _3));
    }

    void whenConnectorIsInvokedWithTimeout()
    {
        using namespace std::placeholders;

        m_connector->connect(
            hpm::api::ConnectResponse(),
            std::chrono::milliseconds(1),
            std::bind(&CloudRelayConnector::onConnectFinished, this, _1, _2, _3));
    }

    void assertConnectorProvidedAConnection()
    {
        auto connectResult = m_connectFinished.get_future().get();
        ASSERT_EQ(hpm::api::NatTraversalResultCode::ok, connectResult.resultCode);
        ASSERT_EQ(SystemError::noError, connectResult.sysErrorCode);
        ASSERT_NE(nullptr, connectResult.connection);
    }

    void assertConnectorReportedError()
    {
        auto connectResult = m_connectFinished.get_future().get();
        ASSERT_NE(hpm::api::NatTraversalResultCode::ok, connectResult.resultCode);
        ASSERT_EQ(nullptr, connectResult.connection);
    }

    void assertTimedOutHasBeenReported()
    {
        auto connectResult = m_connectFinished.get_future().get();
        ASSERT_EQ(
            hpm::api::NatTraversalResultCode::errorConnectingToRelay,
            connectResult.resultCode);
        ASSERT_EQ(SystemError::timedOut, connectResult.sysErrorCode);
        ASSERT_EQ(nullptr, connectResult.connection);
    }

    void assertRequestToRelayServerHasBeenCancelled()
    {
        ASSERT_TRUE(
            m_prevClientToRelayConnectionInstanciated.load() == nullptr ||
            m_prevClientToRelayConnectionInstanciated.load()->scheduledRequestCount() == 0);
    }

private:
    struct Result
    {
        nx::hpm::api::NatTraversalResultCode resultCode;
        SystemError::ErrorCode sysErrorCode;
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection;

        Result(
            nx::hpm::api::NatTraversalResultCode resultCode,
            SystemError::ErrorCode sysErrorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
            :
            resultCode(resultCode),
            sysErrorCode(sysErrorCode),
            connection(std::move(connection))
        {
        }
    };

    std::unique_ptr<relay::Connector> m_connector;
    nx::utils::promise<Result> m_connectFinished;
    SocketAddress m_relayEndpoint;
    std::atomic<ClientToRelayConnection*> m_prevClientToRelayConnectionInstanciated;

    virtual void onClientToRelayConnectionInstanciated(
        ClientToRelayConnection* connection) override
    {
        m_prevClientToRelayConnectionInstanciated = connection;
    }

    virtual void onClientToRelayConnectionDestroyed(
        ClientToRelayConnection* clientToRelayConnection) override
    {
        m_prevClientToRelayConnectionInstanciated.compare_exchange_strong(
            clientToRelayConnection, nullptr);
    }

    void onConnectFinished(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
    {
        m_connectFinished.set_value(Result(resultCode, sysErrorCode, std::move(connection)));
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(CloudRelayConnector, creates_connection_on_success_response_from_relay)
{
    givenHappyRelay();
    whenConnectorIsInvoked();
    assertConnectorProvidedAConnection();
}

TEST_F(CloudRelayConnector, reports_error_on_relay_request_failure)
{
    givenUnhappyRelay();
    whenConnectorIsInvoked();
    assertConnectorReportedError();
}

TEST_F(CloudRelayConnector, reports_timedout_on_relay_request_timeout)
{
    givenSilentRelay();
    whenConnectorIsInvokedWithTimeout();
    assertTimedOutHasBeenReported();
    assertRequestToRelayServerHasBeenCancelled();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
