#include <future>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/uuid.h>

#include <relay/relay_cluster_client.h>
#include <server/hole_punching_processor.h>
#include <settings.h>
#include <statistics/collector.h>
#include <test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"
#include "test_connection.h"

namespace nx {
namespace hpm {
namespace test {

class FtHolePunchingProcessor:
    public MediatorFunctionalTest
{
public:
    enum class StartPolicy
    {
        immediately,
        delayed,
    };

    FtHolePunchingProcessor(StartPolicy startPolicy = StartPolicy::immediately)
    {
        if (startPolicy == StartPolicy::immediately)
        {
            NX_GTEST_ASSERT_TRUE(startAndWaitUntilStarted());
        }

        m_system = addRandomSystem();
    }

    ~FtHolePunchingProcessor()
    {
        if (m_udpClient)
            m_udpClient->pleaseStopSync();
    }

protected:
    void reinitializeUdpClient()
    {
        m_udpClient = std::make_unique<nx::hpm::api::MediatorClientUdpConnection>(stunEndpoint());
    }

    void resetUdpClient()
    {
        m_udpClient->pleaseStopSync();
        m_udpClient.reset();
    }

    nx::hpm::AbstractCloudDataProvider::System m_system;
    std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> m_udpClient;
};

TEST_F(FtHolePunchingProcessor, generic_tests)
{
    const std::chrono::milliseconds kMaxConnectResponseWaitTimeout(15000);

    using namespace nx::hpm;

    const auto server1 = addRandomServer(m_system, boost::none, hpm::ServerTweak::noBindEndpoint);
    ASSERT_NE(nullptr, server1);

    static const std::vector<std::list<nx::network::SocketAddress>> kTestCases =
    {
        {}, // no public addresses
        { server1->endpoint() },
        { server1->endpoint(), nx::network::SocketAddress("12.34.56.78:12345") },
    };

    for (const auto& directTcpAddresses : kTestCases)
    {
        if (directTcpAddresses.size())
            server1->updateTcpAddresses(directTcpAddresses);

        boost::optional<api::ConnectionRequestedEvent> connectionRequestedEventData;
        server1->setOnConnectionRequestedHandler(
            [&connectionRequestedEventData](api::ConnectionRequestedEvent data)
                -> MediaServerEmulator::ActionToTake
            {
                connectionRequestedEventData = std::move(data);
                return MediaServerEmulator::ActionToTake::proceedWithConnection;
            });

        //TODO #ak #msvc2015 use future/promise
        nx::utils::promise<api::ResultCode> connectionAckResultPromise;
        server1->setConnectionAckResponseHandler(
            [&connectionAckResultPromise](api::ResultCode resultCode)
                -> MediaServerEmulator::ActionToTake
            {
                connectionAckResultPromise.set_value(resultCode);
                return MediaServerEmulator::ActionToTake::ignoreIndication;
            });

        const auto listenResult = server1->listen();
        ASSERT_EQ(api::ResultCode::ok, listenResult.first);
        ASSERT_EQ(
            nx::network::KeepAliveOptions(std::chrono::seconds(10), std::chrono::seconds(10), 3),
            listenResult.second.tcpConnectionKeepAlive);

        //requesting connect to the server
        reinitializeUdpClient();

        nx::utils::promise<api::ResultCode> connectResultPromise;

        api::ConnectResponse connectResponseData;
        auto connectCompletionHandler =
            [&connectResultPromise, &connectResponseData](
                nx::network::stun::TransportHeader /*stunTransportHeader*/,
                api::ResultCode resultCode,
                api::ConnectResponse responseData)
            {
                connectResponseData = std::move(responseData);
                connectResultPromise.set_value(resultCode);
            };
        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerId = QnUuid::createUuid().toByteArray();
        connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.destinationHostName = server1->serverId() + "." + m_system.id;
        m_udpClient->connect(
            connectRequest,
            connectCompletionHandler);

        //waiting for connect response and checking server UDP endpoint in response
        ASSERT_EQ(api::ResultCode::ok, connectResultPromise.get_future().get());
        ASSERT_FALSE(connectResponseData.udpEndpointList.empty());
        ASSERT_EQ(
            lm("%1").container(directTcpAddresses).toStdString(),
            lm("%1").container(connectResponseData.forwardedTcpEndpointList).toStdString());
        ASSERT_EQ(
            server1->udpHolePunchingEndpoint().port,
            connectResponseData.udpEndpointList.front().port);

        //checking server has received connectionRequested indication
        ASSERT_TRUE(static_cast<bool>(connectionRequestedEventData));
        ASSERT_EQ(
            connectRequest.originatingPeerId,
            connectionRequestedEventData->originatingPeerID);
        ASSERT_EQ(1U, connectionRequestedEventData->udpEndpointList.size());
        ASSERT_EQ(
            m_udpClient->localAddress().port,
            connectionRequestedEventData->udpEndpointList.front().port);    //not comparing interface since it may be 0.0.0.0 for server

        api::ResultCode resultCode = api::ResultCode::ok;
        api::ConnectionResultRequest connectionResult;
        connectionResult.connectSessionId = connectRequest.connectSessionId;
        std::tie(resultCode) =
            makeSyncCall<api::ResultCode>(
                std::bind(
                    &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                    m_udpClient.get(),
                    std::move(connectionResult),
                    std::placeholders::_1));

        //waiting for connectionAck response to be received by server
        ASSERT_EQ(api::ResultCode::ok, connectionAckResultPromise.get_future().get());

        //testing that mediator has cleaned up session data
        std::tie(resultCode) =
            makeSyncCall<api::ResultCode>(
                std::bind(
                    &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                    m_udpClient.get(),
                    std::move(connectionResult),
                    std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::notFound, resultCode);

        resetUdpClient();
    }
}

TEST_F(FtHolePunchingProcessor, destruction)
{
    const auto server1 = addRandomServer(m_system);

    ASSERT_EQ(api::ResultCode::ok, server1->listen().first);

    for (int i = 0; i < 100; ++i)
    {
        reinitializeUdpClient();

        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerId = QnUuid::createUuid().toByteArray();
        connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.destinationHostName = server1->serverId() + "." + m_system.id;
        nx::utils::promise<void> connectResponsePromise;
        m_udpClient->connect(
            connectRequest,
            [&connectResponsePromise](
                nx::network::stun::TransportHeader /*stunTransportHeader*/,
                api::ResultCode /*resultCode*/,
                api::ConnectResponse /*responseData*/)
            {
                connectResponsePromise.set_value();
            });
        connectResponsePromise.get_future().wait();

        resetUdpClient();
    }
}

//-------------------------------------------------------------------------------------------------

class FtHolePunchingProcessorServerFailure:
    public FtHolePunchingProcessor
{
public:
    FtHolePunchingProcessorServerFailure():
        FtHolePunchingProcessor(StartPolicy::delayed)
    {
        addArg("--cloudConnect/connectionResultWaitTimeout=0s");

        NX_GTEST_ASSERT_TRUE(startAndWaitUntilStarted());
    }

protected:
    void givenSilentServer()
    {
        initialiseServerEmulator(MediaServerEmulator::ActionToTake::ignoreIndication);
    }

    void givenServerThatBreaksConnectionToMediatorOnIndication()
    {
        initialiseServerEmulator(MediaServerEmulator::ActionToTake::closeConnectionToMediator);
    }

    void whenIssueConnectRequest()
    {
        // TODO
    }

    void thenConnectResultCodeIs(api::ResultCode resultCode)
    {
        reinitializeUdpClient();
        assertConnectRequestProducesResult(*m_server, resultCode);
    }

    void assertConnectRequestProducesResult(
        const MediaServerEmulator& mediaServer,
        api::ResultCode expectedResult)
    {
        m_lastConnectSessionId = QnUuid::createUuid().toByteArray();

        nx::utils::promise<api::ResultCode> connectResult;

        api::ConnectResponse connectResponseData;
        auto connectCompletionHandler =
            [&connectResult, &connectResponseData](
                nx::network::stun::TransportHeader /*stunTransportHeader*/,
                api::ResultCode resultCode,
                api::ConnectResponse responseData)
            {
                connectResponseData = std::move(responseData);
                connectResult.set_value(resultCode);
            };
        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerId = QnUuid::createUuid().toByteArray();
        connectRequest.connectSessionId = m_lastConnectSessionId;
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.destinationHostName = mediaServer.serverId() + "." + m_system.id;
        m_udpClient->connect(
            connectRequest,
            connectCompletionHandler);

        //waiting for connect response and checking server UDP endpoint in response
        ASSERT_EQ(expectedResult, connectResult.get_future().get());
    }

    void assertMediatorHasDroppedConnectSession()
    {
        ASSERT_EQ(api::ResultCode::notFound, doConnectResultRequest());
    }

    void waitForMediatorToDropConnectSession()
    {
        while (doConnectResultRequest() != api::ResultCode::notFound)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

private:
    nx::String m_lastConnectSessionId;

    api::ResultCode doConnectResultRequest()
    {
        api::ResultCode resultCode = api::ResultCode::ok;
        api::ConnectionResultRequest connectionResult;
        connectionResult.connectSessionId = m_lastConnectSessionId;
        connectionResult.resultCode = api::NatTraversalResultCode::udtConnectFailed;
        std::tie(resultCode) =
            makeSyncCall<api::ResultCode>(
                std::bind(
                    &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                    m_udpClient.get(),
                    std::move(connectionResult),
                    std::placeholders::_1));
        return resultCode;
    }
    std::unique_ptr<MediaServerEmulator> m_server;

    void initialiseServerEmulator(MediaServerEmulator::ActionToTake actionToTake)
    {
        m_server = addRandomServer(m_system, boost::none, ServerTweak::noBindEndpoint);

        m_server->setOnConnectionRequestedHandler(
            [actionToTake](api::ConnectionRequestedEvent /*data*/)
                -> MediaServerEmulator::ActionToTake
            {
                return actionToTake;
            });

        ASSERT_EQ(api::ResultCode::ok, m_server->listen().first);
    }
};

TEST_F(
    FtHolePunchingProcessorServerFailure,
    proper_error_is_reported_in_case_of_silent_listening_peer)
{
    givenSilentServer();

    whenIssueConnectRequest();

    thenConnectResultCodeIs(api::ResultCode::noSuitableConnectionMethod);
    waitForMediatorToDropConnectSession();
}

TEST_F(
    FtHolePunchingProcessorServerFailure,
    proper_error_is_reported_in_case_of_listening_peer_breaking_connection)
{
    givenServerThatBreaksConnectionToMediatorOnIndication();
    whenIssueConnectRequest();
    thenConnectResultCodeIs(api::ResultCode::notFound);
}

//-------------------------------------------------------------------------------------------------

class DummyStatisticsCollector:
    public stats::AbstractCollector
{
public:
    virtual void saveConnectSessionStatistics(stats::ConnectSession /*data*/) override
    {
    }
};

class TestCloudDataProvider:
    public AbstractCloudDataProvider
{
public:
    virtual boost::optional< AbstractCloudDataProvider::System > getSystem(
        const String& systemId) const override
    {
        auto it = m_systems.find(systemId);
        if (it == m_systems.end())
            return boost::none;
        return it->second;
    }

    void registerSystem(AbstractCloudDataProvider::System system)
    {
        m_systems.emplace(system.id, system);
    }

private:
    std::map<nx::String, AbstractCloudDataProvider::System> m_systems;
};

class HolePunchingProcessor:
    public ::testing::Test
{
    using TestClientConnection = TestConnection<nx::network::UDPSocket>;
    using TestServerConnection = TestTcpConnection;

public:
    HolePunchingProcessor():
        m_listeningPeerPool(m_settings.listeningPeer()),
        m_relayClusterClient(m_settings),
        m_holePunchingProcessor(
            m_settings,
            &m_cloudData,
            &m_listeningPeerPool,
            &m_relayClusterClient,
            &m_statisticsCollector)
    {
        const char* args[] = {
            "--cloudConnect/connectionAckAwaitTimeout=0s",
            "--cloudConnect/connectionResultWaitTimeout=1000m"  //< Equivalent of inifinite session.
        };

        m_settings.load(sizeof(args) / sizeof(*args), args);

        m_originatingPeerName = nx::utils::generateRandomName(7);
    }

protected:
    void emulateRelayPresence()
    {
        m_relayUrl = "http://relay.host/relay/path?relay.query";
        std::array<const char*, 2U> args{
            "-trafficRelay/url",
            m_relayUrl.constData()};

        m_settings.load(args.size(), args.data());
    }

    void setClientCloudConnectVersion(api::CloudConnectVersion version)
    {
        m_clientCloudConnectVersion = version;
    }

    api::CloudConnectVersion clientCloudConnectVersion() const
    {
        return m_clientCloudConnectVersion;
    }

    void givenSilentServer()
    {
        registerListeningPeer();
    }

    void givenConnectInitiationSession()
    {
        whenSentConnectRequest();
    }

    void whenSentConnectRequest()
    {
        using namespace std::placeholders;

        m_originatingPeerConnection = std::make_shared<TestClientConnection>();

        m_connectRequest.connectSessionId = nx::utils::generateRandomName(7);
        m_connectRequest.destinationHostName = m_listeningPeerName;
        m_connectRequest.originatingPeerId = m_originatingPeerName;
        m_connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        m_connectRequest.cloudConnectVersion = m_clientCloudConnectVersion;

        m_holePunchingProcessor.connect(
            m_originatingPeerConnection,
            m_connectRequest,
            nx::network::stun::Message(),
            std::bind(&HolePunchingProcessor::sendConnectResponse, this, _1, _2));
    }

    void whenSentConnectRequestRetransmit()
    {
        using namespace std::placeholders;

        // Waiting initial response.
        m_prevConnectResponse = m_responses.pop();

        m_holePunchingProcessor.connect(
            m_originatingPeerConnection,
            m_connectRequest,
            nx::network::stun::Message(),
            std::bind(&HolePunchingProcessor::sendConnectResponse, this, _1, _2));
    }

    void thenMediatorReportsResult(api::ResultCode expectedResultCode)
    {
        m_prevConnectResponse = m_responses.pop();
        ASSERT_EQ(expectedResultCode, m_prevConnectResponse->first);
    }

    void andNoUdpEndpointIsReported()
    {
        ASSERT_TRUE(static_cast<bool>(m_prevConnectResponse));
        ASSERT_TRUE(m_prevConnectResponse->second.udpEndpointList.empty());
    }

    void andRelayUrlIsReported()
    {
        ASSERT_TRUE(static_cast<bool>(m_prevConnectResponse));
        ASSERT_TRUE(static_cast<bool>(m_prevConnectResponse->second.trafficRelayUrl));
        ASSERT_EQ(m_relayUrl, *m_prevConnectResponse->second.trafficRelayUrl);
    }

    void thenMustReceiveResponseToRetransmit()
    {
        const auto responseToRetransmittedRequest = m_responses.pop();
    }

private:
    using ConnectResult = std::pair<api::ResultCode, api::ConnectResponse>;

    conf::Settings m_settings;
    TestCloudDataProvider m_cloudData;
    ListeningPeerPool m_listeningPeerPool;
    DummyStatisticsCollector m_statisticsCollector;
    RelayClusterClient m_relayClusterClient;
    hpm::HolePunchingProcessor m_holePunchingProcessor;
    nx::utils::SyncQueue<ConnectResult> m_responses;
    nx::String m_relayUrl;

    nx::String m_listeningPeerName;
    std::shared_ptr<TestServerConnection> m_listeningPeerConnection;
    nx::String m_originatingPeerName;
    std::shared_ptr<TestClientConnection> m_originatingPeerConnection;
    api::ConnectRequest m_connectRequest;
    boost::optional<ConnectResult> m_prevConnectResponse;
    api::CloudConnectVersion m_clientCloudConnectVersion =
        api::kCurrentCloudConnectVersion;

    void registerListeningPeer()
    {
        const auto systemId = nx::utils::generateRandomName(7);
        const auto serverId = nx::utils::generateRandomName(7);

        m_listeningPeerName = serverId + "." + systemId;

        AbstractCloudDataProvider::System system;
        system.id = systemId;
        system.mediatorEnabled = true;
        system.authKey = nx::utils::generateRandomName(8);
        m_cloudData.registerSystem(std::move(system));

        m_listeningPeerConnection = std::make_shared<TestServerConnection>();

        MediaserverData listeningPeer;
        listeningPeer.systemId = systemId;
        listeningPeer.serverId = serverId;
        auto locker = m_listeningPeerPool.insertAndLockPeerData(
            m_listeningPeerConnection, listeningPeer);
        locker.value().isListening = true;
        locker.value().cloudConnectVersion = api::kCurrentCloudConnectVersion;
    }

    void sendConnectResponse(api::ResultCode resultCode, api::ConnectResponse response)
    {
        ASSERT_NE(api::ResultCode::notFound, resultCode);
        m_responses.push(ConnectResult(resultCode, std::move(response)));
    }
};

TEST_F(HolePunchingProcessor, response_to_connect_retransmit)
{
    givenSilentServer();
    givenConnectInitiationSession();

    whenSentConnectRequestRetransmit();

    thenMustReceiveResponseToRetransmit();
}

//-------------------------------------------------------------------------------------------------

class HolePunchingProcessorCorrectlyHandlesServersWithoutUdp:
    public HolePunchingProcessor,
    public ::testing::WithParamInterface<api::CloudConnectVersion>
{
};

TEST_P(
    HolePunchingProcessorCorrectlyHandlesServersWithoutUdp,
    connect_is_successful_if_relay_is_present)
{
    emulateRelayPresence();
    givenSilentServer();

    whenSentConnectRequest();

    thenMediatorReportsResult(api::ResultCode::ok);

    if (clientCloudConnectVersion() >=
            api::CloudConnectVersion::clientSupportsConnectSessionWithoutUdpEndpoints)
    {
        andNoUdpEndpointIsReported();
    }

    andRelayUrlIsReported();
}

TEST_P(
    HolePunchingProcessorCorrectlyHandlesServersWithoutUdp,
    connect_is_not_successful_if_relay_is_present)
{
    givenSilentServer();
    whenSentConnectRequest();
    thenMediatorReportsResult(api::ResultCode::noSuitableConnectionMethod);
}

INSTANTIATE_TEST_CASE_P(
    BeforeAddingEmptyUdpEndpointListSupportToClient,
    HolePunchingProcessorCorrectlyHandlesServersWithoutUdp,
    ::testing::Values(api::CloudConnectVersion::serverChecksConnectionState));

INSTANTIATE_TEST_CASE_P(
    AfterAddingEmptyUdpEndpointListSupportToClient,
    HolePunchingProcessorCorrectlyHandlesServersWithoutUdp,
    ::testing::Values(
        api::CloudConnectVersion::clientSupportsConnectSessionWithoutUdpEndpoints));

INSTANTIATE_TEST_CASE_P(
    CurrentClient,
    HolePunchingProcessorCorrectlyHandlesServersWithoutUdp,
    ::testing::Values(api::kCurrentCloudConnectVersion));

} // namespace test
} // namespace hpm
} // namespace nx
