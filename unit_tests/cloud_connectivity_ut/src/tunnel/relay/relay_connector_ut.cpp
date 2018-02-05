#include <atomic>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connector.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

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

    void givenHappyRelay()
    {
        m_relayUrl = nx::utils::Url(lm("http://127.0.0.1:12345"));

        m_connector = std::make_unique<relay::Connector>(
            m_relayUrl,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
    }

    void givenUnhappyRelay()
    {
        m_relayUrl = nx::utils::Url(lm("http://127.0.0.1:12345"));
        m_connector = std::make_unique<relay::Connector>(
            m_relayUrl,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
        m_prevClientToRelayConnectionInstanciated.load()->setFailRequests();
    }

    void givenSilentRelay()
    {
        m_relayUrl = nx::utils::Url(lm("http://127.0.0.1:12345"));

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
        m_prevConnectorResult = m_connectFinished.get_future().get();
        ASSERT_EQ(hpm::api::NatTraversalResultCode::ok, m_prevConnectorResult->resultCode);
        ASSERT_EQ(SystemError::noError, m_prevConnectorResult->sysErrorCode);
        ASSERT_NE(nullptr, m_prevConnectorResult->connection);
    }

    void assertConnectorReportedError()
    {
        m_prevConnectorResult = m_connectFinished.get_future().get();
        ASSERT_NE(hpm::api::NatTraversalResultCode::ok, m_prevConnectorResult->resultCode);
        ASSERT_EQ(nullptr, m_prevConnectorResult->connection);
    }

    void assertTimedOutHasBeenReported()
    {
        m_prevConnectorResult = m_connectFinished.get_future().get();
        ASSERT_EQ(
            hpm::api::NatTraversalResultCode::errorConnectingToRelay,
            m_prevConnectorResult->resultCode);
        ASSERT_EQ(SystemError::timedOut, m_prevConnectorResult->sysErrorCode);
        ASSERT_EQ(nullptr, m_prevConnectorResult->connection);
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

    void createRelayConnector(
        nx::utils::Url relayUrl,
        nx::String targetHostAddress,
        nx::String connectSessionId)
    {
        m_connector = std::make_unique<relay::Connector>(
            relayUrl,
            nx::network::AddressEntry(
                AddressType::cloud, targetHostAddress.constData()),
            connectSessionId);
    }

    const Result& prevConnectorResult()
    {
        return *m_prevConnectorResult;
    }

private:
    std::unique_ptr<relay::Connector> m_connector;
    nx::utils::promise<Result> m_connectFinished;
    nx::utils::Url m_relayUrl;
    std::atomic<nx::cloud::relay::api::test::ClientImpl*>
        m_prevClientToRelayConnectionInstanciated;
    boost::optional<Result> m_prevConnectorResult;

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

//-------------------------------------------------------------------------------------------------
// Test cases.

static const char* const kRelaySessionId = "session1";
static const char* const kServerId = "server1.system1";
static const char* const kRelayApiPrefix = "/RelayConnectorRedirectTest";

class RelayConnectorRedirect:
    public RelayConnector
{
public:
    RelayConnectorRedirect()
    {
        resetClientFactoryToDefault();
    }

protected:
    void whenCreateConnectSessionThatIsRedirectedToAnotherRelayInstance()
    {
        createRelayConnector(
            nx::network::url::Builder().setScheme("http")
                .setEndpoint(m_redirectingRelay.serverAddress()).toUrl(),
            kServerId,
            kRelaySessionId);

        whenConnectorIsInvoked();
        assertConnectorProvidedAConnection();
    }

    void thenProperUrlIsSpecifiedToTunnelConnection()
    {
        using namespace std::placeholders;

        prevConnectorResult().connection->establishNewConnection(
            std::chrono::seconds::zero(),
            SocketAttributes(),
            std::bind(&RelayConnectorRedirect::saveConnectionResult, this, _1, _2, _3));

        ASSERT_EQ(SystemError::noError, m_connectResults.pop().systemErrorCode);
        m_connectionsUpgradedByRealRelay.pop();
    }

private:
    struct ConnectResult
    {
        SystemError::ErrorCode systemErrorCode;
        std::unique_ptr<nx::network::AbstractStreamSocket> connection;
        bool stillValid;
    };

    nx::network::http::TestHttpServer m_redirectingRelay;
    nx::network::http::TestHttpServer m_realRelay;
    nx::utils::SyncQueue<ConnectResult> m_connectResults;
    nx::utils::SyncQueue<int /*dummy*/> m_connectionsUpgradedByRealRelay;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        const auto createClientSessionPath =
            nx::network::http::rest::substituteParameters(
                nx::cloud::relay::api::kServerClientSessionsPath, {kServerId});

        m_realRelay.registerStaticProcessor(
            nx::network::url::normalizePath(
                kRelayApiPrefix + nx::String("/") +
                createClientSessionPath),
            QByteArray("{ \"sessionId\": \"") + kRelaySessionId +
                QByteArray("\", \"sessionTimeout\": \"100\" }"),
            "application/json");

        m_realRelay.registerRequestProcessorFunc(
            nx::network::url::normalizePath(
                kRelayApiPrefix + nx::String("/") +
                nx::network::http::rest::substituteParameters(
                    nx::cloud::relay::api::kClientSessionConnectionsPath, {kRelaySessionId})),
            std::bind(&RelayConnectorRedirect::upgradeConnection, this, _1, _2, _3, _4, _5));

        ASSERT_TRUE(m_realRelay.bindAndListen());

        m_redirectingRelay.registerRedirectHandler(
            createClientSessionPath.c_str(),
            nx::network::url::Builder().setScheme("http")
                .setEndpoint(m_realRelay.serverAddress())
                .setPath(QString::fromStdString(kRelayApiPrefix + createClientSessionPath)));

        ASSERT_TRUE(m_redirectingRelay.bindAndListen());
    }

    void saveConnectionResult(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<nx::network::AbstractStreamSocket> connection,
        bool stillValid)
    {
        m_connectResults.push({systemErrorCode, std::move(connection), stillValid});
    }

    void upgradeConnection(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        completionHandler(nx::network::http::StatusCode::switchingProtocols);
        m_connectionsUpgradedByRealRelay.push(0);
    }
};

TEST_F(RelayConnectorRedirect, handles_redirect_properly)
{
    whenCreateConnectSessionThatIsRedirectedToAnotherRelayInstance();
    thenProperUrlIsSpecifiedToTunnelConnection();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
