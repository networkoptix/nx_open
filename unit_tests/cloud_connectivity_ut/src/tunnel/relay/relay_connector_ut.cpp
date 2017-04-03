#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connector.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

class RelayConnector:
    public ::testing::Test
{
public:
    ~RelayConnector()
    {
        m_connector->pleaseStopSync();
    }

protected:
    void givenSilentRelay()
    {
        m_relayEndpoint = SocketAddress(HostAddress::localhost, 12345);

        m_connector = std::make_unique<relay::Connector>(
            m_relayEndpoint,
            AddressEntry(AddressType::cloud, "any_name"),
            "any_connection_id");
    }

    void whenConnectorIsInvoked()
    {
        using namespace std::placeholders;

        m_connector->connect(
            hpm::api::ConnectResponse(),
            std::chrono::milliseconds(1),
            std::bind(&RelayConnector::onConnectFinished, this, _1, _2, _3));
    }

    void thenTimedOutShouldBeReported()
    {
        auto connectResult = m_connectFinished.get_future().get();
        ASSERT_EQ(
            hpm::api::NatTraversalResultCode::errorConnectingToRelay,
            connectResult.resultCode);
        ASSERT_EQ(SystemError::timedOut, connectResult.sysErrorCode);
    }

private:
    struct Result
    {
        nx::hpm::api::NatTraversalResultCode resultCode/* =
            nx::hpm::api::NatTraversalResultCode::ok*/;
        SystemError::ErrorCode sysErrorCode /*= SystemError::noError*/;
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection;
    };

    std::unique_ptr<relay::Connector> m_connector;
    nx::utils::promise<Result> m_connectFinished;
    SocketAddress m_relayEndpoint;

    void onConnectFinished(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
    {
        m_connectFinished.set_value(Result{resultCode, sysErrorCode, std::move(connection)});
    }
};

TEST_F(RelayConnector, timeout)
{
    givenSilentRelay();
    whenConnectorIsInvoked();
    thenTimedOutShouldBeReported();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
