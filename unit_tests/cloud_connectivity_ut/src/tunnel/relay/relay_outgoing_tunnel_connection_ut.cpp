#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/cloud/tunnel/relay/relay_outgoing_tunnel_connection.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include "api/relay_api_client_stub.h"
#include "cloud_relay_basic_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

using RequestProcessingBehavior = nx::cloud::relay::api::test::RequestProcessingBehavior;

//-------------------------------------------------------------------------------------------------
// Test fixture.

class RelayOutgoingTunnelConnection:
    public BasicFixture
{
    struct Result
    {
        SystemError::ErrorCode sysErrorCode;
        std::unique_ptr<nx::network::AbstractStreamSocket> connection;
        bool stillValid;

        Result():
            sysErrorCode(SystemError::noError),
            stillValid(false)
        {
        }

        Result(
            SystemError::ErrorCode sysErrorCode,
            std::unique_ptr<nx::network::AbstractStreamSocket> connection,
            bool stillValid)
            :
            sysErrorCode(sysErrorCode),
            connection(std::move(connection)),
            stillValid(stillValid)
        {
        }
    };

public:
    RelayOutgoingTunnelConnection():
        m_clientToRelayConnectionCounter(0),
        m_destroyTunnelConnectionOnConnectFailure(false),
        m_relayType(RequestProcessingBehavior::succeed),
        m_connectTimeout(std::chrono::milliseconds::zero()),
        m_connectionsToCreateCount(1)
    {
        m_resultingSocketAttributes.aioThread =
            SocketGlobals::aioService().getRandomAioThread();
        m_resultingSocketAttributes.recvBufferSize =
            nx::utils::random::number<unsigned int>(10000, 50000);
    }

    ~RelayOutgoingTunnelConnection()
    {
        if (m_tunnelConnection)
            m_tunnelConnection->pleaseStopSync();
    }

protected:
    void enableInactivityTimeout()
    {
        m_tunnelInactivityTimeout = std::chrono::milliseconds(1);
    }

    void givenHappyRelay()
    {
        m_relayType = RequestProcessingBehavior::succeed;
    }

    void givenUnhappyRelay()
    {
        m_relayType = RequestProcessingBehavior::fail;
    }

    void givenSilentRelay()
    {
        m_relayType = RequestProcessingBehavior::ignore;
    }

    void givenRelayProducingLogicError()
    {
        m_relayType = RequestProcessingBehavior::produceLogicError;
    }

    void givenIdleTunnel()
    {
        createConnection();
    }

    void whenRequestingConnection()
    {
        m_connectionsToCreateCount = 1;
        requestConnections();
    }

    void whenRequestingMultipleConnection()
    {
        m_connectionsToCreateCount = nx::utils::random::number<int>(2, 10);
        requestConnections();
    }

    void whenRequestingConnectionWithTimeout()
    {
        m_connectTimeout = std::chrono::milliseconds(1);
        whenRequestingConnection();
    }

    void whenReceivedAndSavedConnection()
    {
        whenRequestingConnection();

        m_connection = std::move(m_connectResultQueue.pop().connection);
        ASSERT_NE(nullptr, m_connection);
    }

    void thenRequestToRelayHasBeenIssued()
    {
        ASSERT_GT(m_clientToRelayConnectionCounter.load(), 0);
        // TODO: Check that request has been made
    }

    void thenConnectionIsProvided()
    {
        thenAllConnectionsHaveBeenProvided();
    }

    void thenAllConnectionsHaveBeenProvided()
    {
        for (int i = 0; i < m_connectionsToCreateCount; ++i)
        {
            const auto connectResult = m_connectResultQueue.pop();

            thenRequestToRelayHasBeenIssued();

            ASSERT_EQ(SystemError::noError, connectResult.sysErrorCode);
            ASSERT_NE(nullptr, connectResult.connection);
            ASSERT_TRUE(connectResult.stillValid);
        }
    }

    void thenErrorIsReported()
    {
        thenErrorIsReported(m_connectResultQueue.pop());
    }

    void thenErrorIsReported(const Result& connectResult)
    {
        thenRequestToRelayHasBeenIssued();

        ASSERT_NE(SystemError::noError, connectResult.sysErrorCode);
        ASSERT_EQ(nullptr, connectResult.connection);
        ASSERT_FALSE(connectResult.stillValid);
    }

    void thenTimedOutIsReported()
    {
        const auto connectResult = m_connectResultQueue.pop();
        thenErrorIsReported(connectResult);
        ASSERT_EQ(SystemError::timedOut, connectResult.sysErrorCode);
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

    void thenTunnelIsClosedByTimeout()
    {
        ASSERT_EQ(SystemError::timedOut, m_tunnelClosed.get_future().get());
    }

    void thenTunnelInactivityTimeoutIsNotTriggered()
    {
        ASSERT_EQ(
            std::future_status::timeout,
            m_tunnelClosed.get_future().wait_for(*m_tunnelInactivityTimeout * 3));
    }

    void thenSocketAttributesHaveBeenAppliedToTheResultingSocket()
    {
        ASSERT_TRUE(nx::network::verifySocketAttributes(
            *m_connection, m_resultingSocketAttributes));
    }

    void enableDestroyingTunnelConnectionOnConnectFailure()
    {
        m_destroyTunnelConnectionOnConnectFailure = true;
    }

    void destroyTunnel()
    {
        m_tunnelConnection.reset();
    }

    void destroySavedConnection()
    {
        m_connection.reset();
    }

private:
    std::unique_ptr<relay::OutgoingTunnelConnection> m_tunnelConnection;
    nx::utils::SyncQueue<Result> m_connectResultQueue;
    std::atomic<int> m_clientToRelayConnectionCounter;
    bool m_isRelayHappy;
    nx::utils::promise<SystemError::ErrorCode> m_tunnelClosed;
    bool m_destroyTunnelConnectionOnConnectFailure;
    RequestProcessingBehavior m_relayType = RequestProcessingBehavior::succeed;
    std::chrono::milliseconds m_connectTimeout;
    int m_connectionsToCreateCount;
    boost::optional<std::chrono::milliseconds> m_tunnelInactivityTimeout;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_connection;
    aio::BasicPollable m_aioThreadBinder;
    nx::network::SocketAttributes m_resultingSocketAttributes;

    virtual void onClientToRelayConnectionInstanciated(
        nx::cloud::relay::api::test::ClientImpl* relayClient) override
    {
        relayClient->setBehavior(m_relayType);

        ++m_clientToRelayConnectionCounter;
    }

    void createConnection()
    {
        using namespace std::placeholders;
        using namespace nx::cloud::relay;

        const auto relayUrl = nx::utils::Url("http://127.0.0.1:12345");

        auto clientToRelayConnection = api::ClientFactory::create(relayUrl);

        m_tunnelConnection = std::make_unique<relay::OutgoingTunnelConnection>(
            relayUrl,
            nx::String(),
            std::move(clientToRelayConnection));
        m_tunnelConnection->bindToAioThread(m_aioThreadBinder.getAioThread());
        m_tunnelConnection->setControlConnectionClosedHandler(
            std::bind(&RelayOutgoingTunnelConnection::onTunnelClosed, this, _1));
        if (m_tunnelInactivityTimeout)
            m_tunnelConnection->setInactivityTimeout(*m_tunnelInactivityTimeout);

        m_tunnelConnection->start();
    }

    void onTunnelClosed(SystemError::ErrorCode sysErrorCode)
    {
        ASSERT_TRUE(m_tunnelConnection->isInSelfAioThread());
        m_tunnelClosed.set_value(sysErrorCode);
    }

    void onConnectDone(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<nx::network::AbstractStreamSocket> connection,
        bool stillValid)
    {
        ASSERT_TRUE(m_tunnelConnection->isInSelfAioThread());

        if (m_destroyTunnelConnectionOnConnectFailure)
            m_tunnelConnection.reset();

        m_connectResultQueue.push(
            Result(sysErrorCode, std::move(connection), stillValid));
    }

    void requestConnections()
    {
        using namespace std::placeholders;

        nx::utils::promise<void> done;

        m_aioThreadBinder.post(
            [this, &done]()
            {
                if (!m_tunnelConnection)
                    createConnection();

                for (int i = 0; i < m_connectionsToCreateCount; ++i)
                {
                    m_tunnelConnection->establishNewConnection(
                        m_connectTimeout,
                        m_resultingSocketAttributes,
                        std::bind(&RelayOutgoingTunnelConnection::onConnectDone, this,
                            _1, _2, _3));
                }

                done.set_value();
            });

        done.get_future().wait();
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(RelayOutgoingTunnelConnection, asks_relay_for_connection_using_proper_session_id)
{
    givenHappyRelay();
    whenRequestingConnection();
    thenConnectionIsProvided();
}

TEST_F(RelayOutgoingTunnelConnection, shuts_down_on_receiving_error_from_relay)
{
    givenUnhappyRelay();

    whenRequestingConnection();

    thenErrorIsReported();
    thenTunnelReportedClosure();
}

TEST_F(RelayOutgoingTunnelConnection, tunnel_removed_from_closed_handler)
{
    givenUnhappyRelay();
    enableDestroyingTunnelConnectionOnConnectFailure();

    whenRequestingConnection();

    thenErrorIsReported();
    thenTunnelDidNotReportClosure();
}

TEST_F(RelayOutgoingTunnelConnection, connect_timeout)
{
    givenSilentRelay();

    whenRequestingConnectionWithTimeout();

    thenTimedOutIsReported();
    thenTunnelReportedClosure();
}

TEST_F(RelayOutgoingTunnelConnection, multiple_concurrent_connections)
{
    givenHappyRelay();
    whenRequestingMultipleConnection();
    thenAllConnectionsHaveBeenProvided();
}

TEST_F(RelayOutgoingTunnelConnection, inactivity_timeout_just_after_tunnel_creation)
{
    enableInactivityTimeout();

    givenIdleTunnel();
    thenTunnelIsClosedByTimeout();
}

TEST_F(RelayOutgoingTunnelConnection, inactivity_timeout_just_after_some_activity)
{
    enableInactivityTimeout();

    givenHappyRelay();

    whenRequestingConnection();

    thenConnectionIsProvided();
    thenTunnelIsClosedByTimeout();
}

TEST_F(
    RelayOutgoingTunnelConnection,
    inactivity_timer_does_not_start_while_provided_connections_are_in_use)
{
    enableInactivityTimeout();

    givenHappyRelay();
    whenReceivedAndSavedConnection();
    thenTunnelInactivityTimeoutIsNotTriggered();
}

TEST_F(RelayOutgoingTunnelConnection, provided_connection_destroyed_after_tunnel_connection)
{
    givenHappyRelay();
    whenReceivedAndSavedConnection();
    destroyTunnel();
    destroySavedConnection();
}

TEST_F(RelayOutgoingTunnelConnection, applies_socket_attributes)
{
    givenHappyRelay();
    whenReceivedAndSavedConnection();
    thenSocketAttributesHaveBeenAppliedToTheResultingSocket();
}

TEST_F(RelayOutgoingTunnelConnection, does_not_return_connection_on_error)
{
    // In this case client reports error in still provides connection.
    givenRelayProducingLogicError();
    whenRequestingConnection();
    thenErrorIsReported();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
