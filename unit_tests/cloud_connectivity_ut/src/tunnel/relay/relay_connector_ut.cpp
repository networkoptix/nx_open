#include <atomic>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connector.h>
#include <nx/utils/std/future.h>

#include "api/relay_api_client_stub.h"
#include "cloud_relay_basic_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

static constexpr auto kTimeout = std::chrono::milliseconds(1);

//-------------------------------------------------------------------------------------------------
// Test fixture.

class RelayConnector:
    public BasicFixture
{
public:
    RelayConnector():
        m_prevClientToRelayConnectionInstanciated(nullptr)
    {
    }

    ~RelayConnector()
    {
        m_connector->pleaseStopSync();
    }

protected:
    void givenHappyRelay()
    {
        m_relayUrl = QUrl(lm("http://127.0.0.1:12345"));

        m_connector = std::make_unique<relay::Connector>(
            m_relayUrl,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
    }

    void givenUnhappyRelay()
    {
        m_relayUrl = QUrl(lm("http://127.0.0.1:12345"));
        m_connector = std::make_unique<relay::Connector>(
            m_relayUrl,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
        m_prevClientToRelayConnectionInstanciated.load()->setFailRequests();
    }

    void givenSilentRelay()
    {
        m_relayUrl = QUrl(lm("http://127.0.0.1:12345"));

        m_connector = std::make_unique<relay::Connector>(
            m_relayUrl,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
        m_prevClientToRelayConnectionInstanciated.load()->setIgnoreRequests();
    }

    void whenConnectorIsInvoked()
    {
        using namespace std::placeholders;

        m_connector->connect(
            hpm::api::ConnectResponse(),
            std::chrono::milliseconds::zero(),
            std::bind(&RelayConnector::onConnectFinished, this, _1, _2, _3));
    }

    void whenConnectorIsInvokedWithTimeout()
    {
        using namespace std::placeholders;

        m_connector->connect(
            hpm::api::ConnectResponse(),
            kTimeout,
            std::bind(&RelayConnector::onConnectFinished, this, _1, _2, _3));
    }

    void waitForAnyResponseReported()
    {
        m_connectFinished.get_future().get();
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

    void assertConnectorTimeoutIsNotReported()
    {
        // Reporting timeout reuslts in crash, so just have to wait.
        std::this_thread::sleep_for(kTimeout * 7);
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
    QUrl m_relayUrl;
    std::atomic<nx::cloud::relay::api::test::ClientImpl*>
        m_prevClientToRelayConnectionInstanciated;

    virtual void onClientToRelayConnectionInstanciated(
        nx::cloud::relay::api::test::ClientImpl* connection) override
    {
        m_prevClientToRelayConnectionInstanciated = connection;
    }

    virtual void onClientToRelayConnectionDestroyed(
        nx::cloud::relay::api::test::ClientImpl* clientToRelayConnection) override
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

TEST_F(RelayConnector, creates_connection_on_success_response_from_relay)
{
    givenHappyRelay();
    whenConnectorIsInvoked();
    assertConnectorProvidedAConnection();
}

TEST_F(RelayConnector, reports_error_on_relay_request_failure)
{
    givenUnhappyRelay();
    whenConnectorIsInvoked();
    assertConnectorReportedError();
}

TEST_F(RelayConnector, reports_timedout_on_relay_request_timeout)
{
    givenSilentRelay();
    whenConnectorIsInvokedWithTimeout();
    assertTimedOutHasBeenReported();
    assertRequestToRelayServerHasBeenCancelled();
}

TEST_F(RelayConnector, stops_internal_timer_when_reporting_result)
{
    givenHappyRelay();
    whenConnectorIsInvokedWithTimeout();
    waitForAnyResponseReported();
    assertConnectorTimeoutIsNotReported();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
