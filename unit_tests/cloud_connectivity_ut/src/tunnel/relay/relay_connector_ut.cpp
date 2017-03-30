#include <atomic>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connector.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

class ClientToRelayConnection:
    public api::ClientToRelayConnection
{
    using base_type = api::ClientToRelayConnection;

public:
    ClientToRelayConnection():
        m_scheduledRequestCount(0),
        m_ignoreRequests(false),
        m_failRequests(false)
    {
    }

    virtual ~ClientToRelayConnection()
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();

        // Notifying test fixture
        auto hander = std::move(m_onBeforeDestruction);
        if (hander)
            hander();
    }

    virtual void startSession(
        const nx::String& desiredSessionId,
        const nx::String& /*targetPeerName*/,
        api::StartClientConnectSessionHandler handler) override
    {
        ++m_scheduledRequestCount;
        if (m_ignoreRequests)
            return;

        post(
            [this, handler = std::move(handler), desiredSessionId]()
            {
                if (m_failRequests)
                    handler(api::ResultCode::networkError, desiredSessionId);
                else
                    handler(api::ResultCode::ok, desiredSessionId);
            });
    }

    virtual void openConnectionToTheTargetHost(
        nx::String& /*sessionId*/,
        api::OpenRelayConnectionHandler handler) override
    {
        ++m_scheduledRequestCount;
        if (m_ignoreRequests)
            return;

        post(
            [this, handler = std::move(handler)]()
            {
                if (m_failRequests)
                    handler(api::ResultCode::networkError, nullptr);
                else
                    handler(api::ResultCode::ok, std::make_unique<nx::network::TCPSocket>());
            });
    }

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override
    {
        return SystemError::noError;
    }

    int scheduledRequestCount() const
    {
        return m_scheduledRequestCount;
    }

    void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_onBeforeDestruction = std::move(handler);
    }

    void setIgnoreRequests(bool val)
    {
        m_ignoreRequests = val;
    }

    void setFailRequests(bool val)
    {
        m_failRequests = val;
    }

private:
    int m_scheduledRequestCount;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestruction;
    bool m_ignoreRequests;
    bool m_failRequests;

    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();
        m_scheduledRequestCount = 0;
    }
};

//-------------------------------------------------------------------------------------------------
// Test fixture

class RelayConnector:
    public ::testing::Test
{
public:
    RelayConnector():
        m_prevClientToRelayConnectionInstanciated(nullptr)
    {
        using namespace std::placeholders;

        m_clientToRelayConnectionFactoryBak = 
            api::ClientToRelayConnectionFactory::setCustomFactoryFunc(
                std::bind(&RelayConnector::clientToRelayConnectionFactoryFunc, this, _1));
    }

    ~RelayConnector()
    {
        m_connector->pleaseStopSync();

        api::ClientToRelayConnectionFactory::setCustomFactoryFunc(
            std::move(m_clientToRelayConnectionFactoryBak));
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
            std::bind(&RelayConnector::onConnectFinished, this, _1, _2, _3));
    }

    void whenConnectorIsInvokedWithTimeout()
    {
        using namespace std::placeholders;

        m_connector->connect(
            hpm::api::ConnectResponse(),
            std::chrono::milliseconds(1),
            std::bind(&RelayConnector::onConnectFinished, this, _1, _2, _3));
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

    api::ClientToRelayConnectionFactory::CustomFactoryFunc m_clientToRelayConnectionFactoryBak;
    std::unique_ptr<relay::Connector> m_connector;
    nx::utils::promise<Result> m_connectFinished;
    SocketAddress m_relayEndpoint;
    std::atomic<ClientToRelayConnection*> m_prevClientToRelayConnectionInstanciated;

    std::unique_ptr<api::ClientToRelayConnection> clientToRelayConnectionFactoryFunc(
        const SocketAddress& /*relayEndpoint*/)
    {
        auto result = std::make_unique<ClientToRelayConnection>();
        result->setOnBeforeDestruction(
            std::bind(&RelayConnector::clientToRelayConnectionDestroyed, this, result.get()));
        m_prevClientToRelayConnectionInstanciated = result.get();
        return result;
    }

    void clientToRelayConnectionDestroyed(ClientToRelayConnection* clientToRelayConnection)
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
// Test cases

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

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
