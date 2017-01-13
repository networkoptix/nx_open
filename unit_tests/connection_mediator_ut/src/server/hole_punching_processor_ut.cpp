/**********************************************************
* Jan 19, 2016
* akolesnikov
***********************************************************/

#include <future>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/utils/uuid.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/sync_queue.h>
#include <utils/common/sync_call.h>

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
    enum StartPolicy
    {
        immediately,
        delayed,
    };

    FtHolePunchingProcessor(StartPolicy startPolicy = StartPolicy::immediately)
    {
        if (startPolicy == immediately)
        {
            NX_GTEST_ASSERT_TRUE(startAndWaitUntilStarted());
        }
    }
};

TEST_F(FtHolePunchingProcessor, generic_tests)
{
    const std::chrono::milliseconds kMaxConnectResponseWaitTimeout(15000);

    using namespace nx::hpm;

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1, boost::none, hpm::ServerTweak::noBindEndpoint);
    ASSERT_NE(nullptr, server1);

    static const std::vector<std::list<SocketAddress>> kTestCases =
    {
        {}, // no public addresses
        { server1->endpoint() },
        { server1->endpoint(), SocketAddress("12.34.56.78:12345") },
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
        std::promise<api::ResultCode> connectionAckResultPromise;
        server1->setConnectionAckResponseHandler(
            [&connectionAckResultPromise](api::ResultCode resultCode)
                -> MediaServerEmulator::ActionToTake
            {
                connectionAckResultPromise.set_value(resultCode);
                return MediaServerEmulator::ActionToTake::ignoreIndication;
            });

        const auto listenResult = server1->listen();
        ASSERT_EQ(api::ResultCode::ok, listenResult.first);
        ASSERT_EQ(KeepAliveOptions(10, 10, 3), listenResult.second.tcpConnectionKeepAlive);

        //requesting connect to the server
        nx::hpm::api::MediatorClientUdpConnection udpClient(stunEndpoint());

        std::promise<api::ResultCode> connectResultPromise;

        api::ConnectResponse connectResponseData;
        auto connectCompletionHandler =
            [&connectResultPromise, &connectResponseData](
                stun::TransportHeader /*stunTransportHeader*/,
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
        connectRequest.destinationHostName = server1->serverId() + "." + system1.id;
        udpClient.connect(
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
        ASSERT_EQ(1, connectionRequestedEventData->udpEndpointList.size());
        ASSERT_EQ(
            udpClient.localAddress().port,
            connectionRequestedEventData->udpEndpointList.front().port);    //not comparing interface since it may be 0.0.0.0 for server

        api::ResultCode resultCode = api::ResultCode::ok;
        api::ConnectionResultRequest connectionResult;
        connectionResult.connectSessionId = connectRequest.connectSessionId;
        std::tie(resultCode) =
            makeSyncCall<api::ResultCode>(
                std::bind(
                    &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                    &udpClient,
                    std::move(connectionResult),
                    std::placeholders::_1));

        //waiting for connectionAck response to be received by server
        ASSERT_EQ(api::ResultCode::ok, connectionAckResultPromise.get_future().get());

        //testing that mediator has cleaned up session data
        std::tie(resultCode) =
            makeSyncCall<api::ResultCode>(
                std::bind(
                    &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                    &udpClient,
                    std::move(connectionResult),
                    std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::notFound, resultCode);

        udpClient.pleaseStopSync();
    }
}

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
};

TEST_F(FtHolePunchingProcessorServerFailure, server_failure)
{
    const std::chrono::milliseconds kMaxConnectResponseWaitTimeout(15000);

    using namespace nx::hpm;

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1, boost::none);

    typedef MediaServerEmulator::ActionToTake MsAction;
    static const std::map<MsAction, api::ResultCode> kTestCases =
    {
        { MsAction::ignoreIndication, api::ResultCode::noReplyFromServer },
        { MsAction::closeConnectionToMediator, api::ResultCode::notFound },
    };

    for (const auto& testCase: kTestCases)
    {
        const auto actionToTake = testCase.first;
        const auto expectedResult = testCase.second;

        //TODO #ak #msvc2015 use future/promise
        QnMutex mtx;
        QnWaitCondition waitCond;

        boost::optional<api::ConnectionRequestedEvent> connectionRequestedEventData;
        server1->setOnConnectionRequestedHandler(
            [&connectionRequestedEventData, actionToTake](api::ConnectionRequestedEvent data)
                -> MediaServerEmulator::ActionToTake
            {
                connectionRequestedEventData = std::move(data);
                return actionToTake;
            });

        ASSERT_EQ(api::ResultCode::ok, server1->listen().first);

        //requesting connect to the server 
        nx::hpm::api::MediatorClientUdpConnection udpClient(stunEndpoint());

        boost::optional<api::ResultCode> connectResult;

        api::ConnectResponse connectResponseData;
        auto connectCompletionHandler =
            [&mtx, &waitCond, &connectResult, &connectResponseData](
                stun::TransportHeader /*stunTransportHeader*/,
                api::ResultCode resultCode,
                api::ConnectResponse responseData)
            {
                QnMutexLocker lk(&mtx);
                connectResult = resultCode;
                connectResponseData = std::move(responseData);
                waitCond.wakeAll();
            };
        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerId = QnUuid::createUuid().toByteArray();
        connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.destinationHostName = server1->serverId() + "." + system1.id;
        udpClient.connect(
            connectRequest,
            connectCompletionHandler);

        //waiting for connect response and checking server UDP endpoint in response
        {
            QnMutexLocker lk(&mtx);
            while (!static_cast<bool>(connectResult))
                ASSERT_TRUE(waitCond.wait(lk.mutex(), kMaxConnectResponseWaitTimeout.count()));
        }

        const auto connectResultVal = connectResult.get();
        ASSERT_EQ(expectedResult, connectResultVal);

        if (actionToTake != MsAction::closeConnectionToMediator)
        {
            //testing that mediator has cleaned up session data
            api::ResultCode resultCode = api::ResultCode::ok;
            api::ConnectionResultRequest connectionResult;
            connectionResult.connectSessionId = connectRequest.connectSessionId;
            connectionResult.resultCode = api::NatTraversalResultCode::udtConnectFailed;
            std::tie(resultCode) =
                makeSyncCall<api::ResultCode>(
                    std::bind(
                        &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                        &udpClient,
                        std::move(connectionResult),
                        std::placeholders::_1));
            ASSERT_EQ(api::ResultCode::notFound, resultCode);
        }

        udpClient.pleaseStopSync();
    }
}

TEST_F(FtHolePunchingProcessor, destruction)
{
    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1);

    ASSERT_EQ(api::ResultCode::ok, server1->listen().first);

    for (int i = 0; i < 100; ++i)
    {
        nx::hpm::api::MediatorClientUdpConnection udpClient(stunEndpoint());

        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerId = QnUuid::createUuid().toByteArray();
        connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.destinationHostName = server1->serverId() + "." + system1.id;
        std::promise<void> connectResponsePromise;
        udpClient.connect(
            connectRequest,
            [&connectResponsePromise](
                stun::TransportHeader /*stunTransportHeader*/,
                api::ResultCode /*resultCode*/,
                api::ConnectResponse /*responseData*/)
            {
                connectResponsePromise.set_value();
            });
        connectResponsePromise.get_future().wait();

        udpClient.pleaseStopSync();
    }
}

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
    using TestServerConnection = TestConnection<nx::network::TCPSocket>;

public:
    HolePunchingProcessor():
        m_holePunchingProcessor(
            m_settings,
            &m_cloudData,
            &m_dispatcher,
            &m_listeningPeerPool,
            &m_statisticsCollector)
    {
        char* args[] = {
            "--cloudConnect/connectionAckAwaitTimeout=0s",
            "--cloudConnect/connectionResultWaitTimeout=1000m"  //< Equivalent of inifinite session.
        };

        m_settings.load(sizeof(args) / sizeof(*args), args);

        m_originatingPeerName = nx::utils::generateRandomName(7);
    }

protected:
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

        m_holePunchingProcessor.connect(
            m_originatingPeerConnection,
            m_connectRequest,
            nx::stun::Message(),
            std::bind(&HolePunchingProcessor::sendConnectResponse, this, _1, _2));
    }

    void whenSentConnectRequestRetransmit()
    {
        using namespace std::placeholders;

        // Waiting initial response.
        m_connectResponse = m_responses.pop();

        m_holePunchingProcessor.connect(
            m_originatingPeerConnection,
            m_connectRequest,
            nx::stun::Message(),
            std::bind(&HolePunchingProcessor::sendConnectResponse, this, _1, _2));
    }

    void thenMediatorResponseMustBe(api::ResultCode expectedResultCode)
    {
        m_connectResponse = m_responses.pop();
        ASSERT_EQ(expectedResultCode, m_connectResponse.first);
    }

    void thenMustReceiveResponseToRetransmit()
    {
        const auto responseToRetransmittedRequest = m_responses.pop();
    }

private:
    using ConnectResponse = std::pair<api::ResultCode, api::ConnectResponse>;

    conf::Settings m_settings;
    TestCloudDataProvider m_cloudData;
    nx::stun::MessageDispatcher m_dispatcher;
    ListeningPeerPool m_listeningPeerPool;
    DummyStatisticsCollector m_statisticsCollector;
    hpm::HolePunchingProcessor m_holePunchingProcessor;
    nx::utils::SyncQueue<ConnectResponse> m_responses;

    nx::String m_listeningPeerName;
    std::shared_ptr<TestServerConnection> m_listeningPeerConnection;
    nx::String m_originatingPeerName;
    std::shared_ptr<TestClientConnection> m_originatingPeerConnection;
    api::ConnectRequest m_connectRequest;
    ConnectResponse m_connectResponse;

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
        locker.value().connectionMethods |= api::ConnectionMethod::udpHolePunching;
    }

    void sendConnectResponse(api::ResultCode resultCode, api::ConnectResponse response)
    {
        ASSERT_NE(api::ResultCode::notFound, resultCode);
        m_responses.push(ConnectResponse(resultCode, std::move(response)));
    }
};

TEST_F(HolePunchingProcessor, correct_response_in_case_of_silent_server)
{
    givenSilentServer();
    whenSentConnectRequest();
    thenMediatorResponseMustBe(api::ResultCode::noReplyFromServer);
}

TEST_F(HolePunchingProcessor, response_to_connect_retransmit)
{
    givenSilentServer();
    givenConnectInitiationSession();

    whenSentConnectRequestRetransmit();

    thenMustReceiveResponseToRetransmit();
}

} // namespace test
} // namespace hpm
} // namespace nx
