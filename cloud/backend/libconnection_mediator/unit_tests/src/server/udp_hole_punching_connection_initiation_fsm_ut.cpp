#include <memory>
#include <tuple>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/test_support/program_arguments.h>

#include <nx/cloud/mediator/relay/relay_cluster_client.h>
#include <nx/cloud/mediator/server/udp_hole_punching_connection_initiation_fsm.h>
#include <nx/cloud/mediator/settings.h>

#include "test_connection.h"

namespace nx {
namespace hpm {
namespace test {

namespace {

class TestRelayClusterClient:
    public AbstractRelayClusterClient
{
public:
    virtual void selectRelayInstanceForListeningPeer(
        const std::string& /*peerId*/,
        const nx::network::HostAddress& /*serverHost*/,
        RelayInstanceSelectCompletionHandler /*completionHandler*/)
    {
        // Unused.
    }

    virtual void findRelayInstanceForClient(
        const std::string& /*peerId*/,
        const nx::network::HostAddress& /*clientHost*/,
        RelayInstanceSearchCompletionHandler completionHandler)
    {
        m_pendingRequestHandler = std::move(completionHandler);
        if (!m_responseDelay)
            return;

        auto timer = std::make_unique<nx::network::aio::Timer>();
        auto timerPtr = timer.get();
        timerPtr->start(
            *m_responseDelay,
            [timer = std::move(timer),
                handler = std::exchange(m_pendingRequestHandler, nullptr)]() mutable
            {
                handler(
                    cloud::relay::api::ResultCode::ok,
                    nx::utils::Url("https://relay.vmsproxy.com"));
                timer.reset();
            });
    }

    void setResponseDelay(std::chrono::milliseconds delay)
    {
        m_responseDelay = delay;
    }

    void reportFailure()
    {
        if (m_pendingRequestHandler)
        {
            nx::utils::swapAndCall(
                m_pendingRequestHandler,
                cloud::relay::api::ResultCode::networkError,
                nx::utils::Url());
        }
    }

private:
    RelayInstanceSearchCompletionHandler m_pendingRequestHandler;
    std::optional<std::chrono::milliseconds> m_responseDelay;
};

} // namespace

//-------------------------------------------------------------------------------------------------

using TestServerConnection = TestTcpConnection;

class UDPHolePunchingConnectionInitiationFsm:
    public ::testing::Test
{
public:
    UDPHolePunchingConnectionInitiationFsm():
        m_trafficRelayUrl("http://relay.vmsproxy.com")
    {
        using namespace std::placeholders;

        m_programArguments.addArg("-trafficRelay/url", m_trafficRelayUrl.c_str());

        m_connectSessionId = nx::utils::generateRandomName(7);
        m_listeningPeerConnection = std::make_shared<TestServerConnection>();

        m_listeningPeerFullName = lm("%1.%2")
            .arg(QnUuid::createUuid().toSimpleString())
            .arg(QnUuid::createUuid().toSimpleString());
    }

    ~UDPHolePunchingConnectionInitiationFsm()
    {
        m_connectSessionFsm->pleaseStopSync();
    }

    void initializeConnectSessionFsm()
    {
        if (!m_programArguments.args().empty())
            m_settings.load(m_programArguments.argc(), (const char**) m_programArguments.argv());

        if (!m_relayClusterClient)
            m_relayClusterClient = std::make_unique<RelayClusterClient>(m_settings);

        ListeningPeerData listeningPeerData;
        listeningPeerData.hostName = m_listeningPeerFullName.toUtf8();
        listeningPeerData.peerConnection = m_listeningPeerConnection;
        m_connectSessionFsm = std::make_unique<hpm::UDPHolePunchingConnectionInitiationFsm>(
            m_connectSessionId,
            listeningPeerData,
            [this](auto&&... args) { connectFsmFinished(std::move(args)...); },
            m_settings,
            m_relayClusterClient.get());
    }

protected:
    void givenSessionConnectedThroughTcp()
    {
        whenIssueConnectRequestOverTcp();

        thenConnectResultIsReported();
        andConnectionResultIsSuccess();
    }

    void givenCompletedSession()
    {
        whenIssueConnectRequestOverUdp();
        whenConnectionAckIsReceived();
        thenConnectResultIsReported();
    }

    void whenIssueConnectRequestOverUdp()
    {
        m_connectingPeerConnection =
            std::make_shared<TestConnection<nx::network::UDPSocket>>();

        whenIssueConnectRequest();
    }

    void whenIssueConnectRequestOverTcp()
    {
        m_connectingPeerConnection =
            std::make_shared<TestConnection<nx::network::TCPSocket>>();

        whenIssueConnectRequest();
    }

    void whenIssueConnectRequest()
    {
        if (!m_connectSessionFsm)
            initializeConnectSessionFsm();

        api::ConnectRequest request;
        request.connectSessionId = m_connectSessionId;
        request.connectionMethods = api::ConnectionMethod::all;
        m_connectSessionFsm->onConnectRequest(
            RequestSourceDescriptor{
                m_connectingPeerConnection->transportProtocol(),
                m_connectingPeerConnection->getSourceAddress()},
            std::move(request),
            [this](auto... args) { saveConnectResponse(std::move(args)...); });
    }

    void whenClientSendsResultReport()
    {
        api::ConnectionResultRequest request;
        request.connectSessionId = m_connectSessionId;
        request.resultCode = api::NatTraversalResultCode::noResponseFromMediator;
        m_connectSessionFsm->onConnectionResultRequest(
            std::move(request),
            [](api::ResultCode) {});
    }

    void whenRequestConnectionToThePeerUsingDomainName()
    {
        api::ConnectionAckRequest request;
        request.connectionMethods = api::ConnectionMethod::all;
        request.udpEndpointList.push_back(
            nx::network::SocketAddress(
                nx::network::HostAddress::localhost, 12345)); //< Just any port.
        m_connectSessionFsm->onConnectionAckRequest(
            RequestSourceDescriptor{
                network::TransportProtocol::tcp,
                network::SocketAddress()},
            std::move(request),
            [](api::ResultCode) {});
    }

    void whenConnectionAckIsReceived()
    {
        whenRequestConnectionToThePeerUsingDomainName();
    }

    void whenRelayClusterClientReturns()
    {
        static_cast<TestRelayClusterClient*>(m_relayClusterClient.get())->reportFailure();
    }

    void whenConnectSessionResultIsReceived()
    {
        m_connectResultRequest.resultCode = api::NatTraversalResultCode::endpointVerificationFailure;
        m_connectSessionFsm->onConnectionResultRequest(
            m_connectResultRequest,
            [this](auto&&... args) { saveConnectResultResponse(std::move(args)...); });
    }

    void whenServerConnectionIsBroken()
    {
        m_listeningPeerConnection.reset();
    }

    void thenConnectSessionFsmIsTerminatedProperly()
    {
        m_fsmFinishedPromise.get_future().wait();
    }

    void thenConnectResultIsReported()
    {
        m_prevConnectResult = m_connectResponseQueue.pop();
    }

    void andConnectionResultIsSuccess()
    {
        ASSERT_EQ(api::ResultCode::ok, std::get<0>(*m_prevConnectResult));
    }

    void andResponseContainsRelayInfo()
    {
        const auto& connectResponse = std::get<1>(*m_prevConnectResult);
        ASSERT_TRUE(connectResponse.trafficRelayUrl);
        ASSERT_EQ(m_trafficRelayUrl, connectResponse.trafficRelayUrl->toStdString());
    }

    void andResponseContainsServerUdpEndpoints()
    {
        const auto& connectResponse = std::get<1>(*m_prevConnectResult);
        ASSERT_TRUE(!connectResponse.udpEndpointList.empty());
    }

    void andResponseDoesNotContainUdpEndpoints()
    {
        const auto& connectResponse = std::get<1>(*m_prevConnectResult);
        ASSERT_TRUE(connectResponse.udpEndpointList.empty());
    }

    void thenFullPeerNameHasBeenReportedInResponse()
    {
        thenConnectResultIsReported();

        ASSERT_EQ(
            m_listeningPeerFullName,
            std::get<1>(*m_prevConnectResult).destinationHostFullName);
    }

    void thenRelayInstanceInformationIsIgnored()
    {
        // TODO
    }

    void thenConnectSessionResultResponseIsSuccess()
    {
        ASSERT_EQ(api::ResultCode::ok, m_saveConnectResultResponseQueue.pop());
    }

    void andConnectSessionResultCodeIsAvailableInSessionStatistics()
    {
        ASSERT_EQ(
            m_connectResultRequest.resultCode,
            m_connectSessionFsm->statisticsInfo().resultCode);
    }

    void setSetting(const char* name, std::chrono::milliseconds timeout)
    {
        m_programArguments.addArg(
            name,
            lm("%1ms").arg(timeout.count()).toStdString().c_str());
    }

    void installRelayClusterClientThatRespondsOnCommand()
    {
        m_relayClusterClient = std::make_unique<TestRelayClusterClient>();
    }

    void installRelayClusterClientWithDelay(std::chrono::milliseconds delay)
    {
        auto client = std::make_unique<TestRelayClusterClient>();
        client->setResponseDelay(delay);
        m_relayClusterClient = std::move(client);
    }

    template<typename Func>
    void doInAioThread(Func func)
    {
        if (!m_connectSessionFsm)
            initializeConnectSessionFsm();

        m_connectSessionFsm->executeInAioThreadSync(
            [func = std::move(func)]() { func(); });
    }

    const conf::Settings& settings() const
    {
        return m_settings;
    }

private:
    using ConnectResult = std::tuple<api::ResultCode, api::ConnectResponse>;

    std::unique_ptr<hpm::UDPHolePunchingConnectionInitiationFsm> m_connectSessionFsm;
    std::shared_ptr<TestServerConnection> m_listeningPeerConnection;
    std::shared_ptr<nx::network::stun::AbstractServerConnection> m_connectingPeerConnection;
    conf::Settings m_settings;
    std::unique_ptr<AbstractRelayClusterClient> m_relayClusterClient;
    QByteArray m_connectSessionId;
    nx::utils::promise<void> m_fsmFinishedPromise;
    nx::utils::SyncQueue<ConnectResult> m_connectResponseQueue;
    boost::optional<ConnectResult> m_prevConnectResult;
    api::ConnectionResultRequest m_connectResultRequest;
    nx::utils::SyncQueue<api::ResultCode> m_saveConnectResultResponseQueue;
    QString m_listeningPeerFullName;
    std::vector<const char*> m_args;
    nx::utils::test::ProgramArguments m_programArguments;
    std::string m_trafficRelayUrl;

    void connectFsmFinished(api::NatTraversalResultCode /*resultCode*/)
    {
        m_fsmFinishedPromise.set_value();
    }

    void saveConnectResponse(api::ResultCode result, api::ConnectResponse response)
    {
        m_connectResponseQueue.push(std::make_tuple(result, std::move(response)));
    }

    void saveConnectResultResponse(api::ResultCode resultCode)
    {
        m_saveConnectResultResponseQueue.push(resultCode);
    }
};

TEST_F(
    UDPHolePunchingConnectionInitiationFsm,
    client_sends_connection_result_report_before_receiving_connect_response)
{
    setSetting("-cloudConnect/connectionAckAwaitTimeout", std::chrono::milliseconds(10));
    setSetting("-cloudConnect/connectionResultWaitTimeout", std::chrono::milliseconds(10));

    whenIssueConnectRequestOverUdp();
    whenClientSendsResultReport();
    thenConnectSessionFsmIsTerminatedProperly();
}

TEST_F(UDPHolePunchingConnectionInitiationFsm, remote_peer_full_name_is_reported)
{
    whenIssueConnectRequestOverUdp();

    whenRequestConnectionToThePeerUsingDomainName();

    thenFullPeerNameHasBeenReportedInResponse();
    andConnectionResultIsSuccess();
}

TEST_F(UDPHolePunchingConnectionInitiationFsm, find_a_relay_instance_fails_by_timeout)
{
    setSetting("-cloudConnect/maxRelayInstanceSearchTime", std::chrono::milliseconds(1));
    installRelayClusterClientThatRespondsOnCommand();

    whenIssueConnectRequestOverUdp();

    whenConnectionAckIsReceived();

    thenConnectResultIsReported();
    andConnectionResultIsSuccess();
}

TEST_F(UDPHolePunchingConnectionInitiationFsm, find_a_relay_instance_takes_a_long_time)
{
    setSetting("-cloudConnect/connectionAckAwaitTimeout", std::chrono::milliseconds(1));
    installRelayClusterClientThatRespondsOnCommand();

    doInAioThread(
        [this]()
        {
            whenIssueConnectRequestOverUdp();
            whenConnectionAckIsReceived();
        });

    thenConnectResultIsReported();
    whenRelayClusterClientReturns();

    thenRelayInstanceInformationIsIgnored();
}

TEST_F(UDPHolePunchingConnectionInitiationFsm, connect_over_udp)
{
    whenIssueConnectRequestOverUdp();
    whenConnectionAckIsReceived();

    thenConnectResultIsReported();
    andConnectionResultIsSuccess();
    andResponseContainsRelayInfo();
    andResponseContainsServerUdpEndpoints();
}

TEST_F(UDPHolePunchingConnectionInitiationFsm, connect_over_tcp)
{
    whenIssueConnectRequestOverTcp();

    thenConnectResultIsReported();
    andConnectionResultIsSuccess();
    andResponseContainsRelayInfo();
    andResponseDoesNotContainUdpEndpoints();
}

TEST_F(
    UDPHolePunchingConnectionInitiationFsm,
    response_to_connect_retransmit_over_tcp_does_not_contain_udp_endpoints)
{
    givenCompletedSession();

    whenIssueConnectRequestOverTcp();

    thenConnectResultIsReported();
    andResponseDoesNotContainUdpEndpoints();
}

TEST_F(
    UDPHolePunchingConnectionInitiationFsm,
    connect_session_result_is_properly_recorded_for_session_connected_through_tcp)
{
    givenSessionConnectedThroughTcp();

    whenConnectSessionResultIsReceived();

    thenConnectSessionResultResponseIsSuccess();
    andConnectSessionResultCodeIsAvailableInSessionStatistics();
}

TEST_F(UDPHolePunchingConnectionInitiationFsm, connect_to_server_with_broken_connection)
{
    setSetting("-cloudConnect/connectionAckAwaitTimeout", std::chrono::milliseconds(1));
    installRelayClusterClientWithDelay(std::chrono::milliseconds(10));

    whenServerConnectionIsBroken();
    whenIssueConnectRequestOverUdp();

    std::this_thread::sleep_for(
        settings().connectionParameters().connectionAckAwaitTimeout * 2);

    whenIssueConnectRequest();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // then application does not crash.
}

TEST_F(UDPHolePunchingConnectionInitiationFsm, connection_ack_arrives_after_no_ack_timeout)
{
    setSetting("-cloudConnect/connectionAckAwaitTimeout", std::chrono::milliseconds(1));

    installRelayClusterClientWithDelay(std::chrono::milliseconds(10));

    whenIssueConnectRequestOverUdp();

    std::this_thread::sleep_for(
        settings().connectionParameters().connectionAckAwaitTimeout * 2);

    whenConnectionAckIsReceived();

    thenConnectResultIsReported();
    andConnectionResultIsSuccess();
}

} // namespace test
} // namespace hpm
} // namespace nx
