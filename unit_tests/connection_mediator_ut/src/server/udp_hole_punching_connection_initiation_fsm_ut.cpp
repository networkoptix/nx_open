/**********************************************************
* Jan 19, 2016
* akolesnikov
***********************************************************/

#include <memory>
#include <tuple>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <relay/relay_cluster_client.h>
#include <server/udp_hole_punching_connection_initiation_fsm.h>
#include <settings.h>

#include "test_connection.h"

namespace nx {
namespace hpm {
namespace test {

using TestServerConnection = TestConnection<nx::network::TCPSocket>;

class UDPHolePunchingConnectionInitiationFsm:
    public ::testing::Test
{
public:
    UDPHolePunchingConnectionInitiationFsm():
        m_relayClusterClient(m_settings)
    {
        using namespace std::placeholders;

        m_connectSessionId = nx::utils::generateRandomName(7);
        m_listeningPeerConnection = std::make_shared<TestServerConnection>();

        m_listeningPeerFullName = lm("%1.%2")
            .arg(QnUuid::createUuid().toSimpleString())
            .arg(QnUuid::createUuid().toSimpleString());

        ListeningPeerData listeningPeerData;
        listeningPeerData.hostName = m_listeningPeerFullName.toUtf8();
        listeningPeerData.peerConnection = m_listeningPeerConnection;
        m_connectSessionFsm = std::make_unique<hpm::UDPHolePunchingConnectionInitiationFsm>(
            m_connectSessionId,
            listeningPeerData,
            std::bind(&UDPHolePunchingConnectionInitiationFsm::connectFsmFinished, this, _1),
            m_settings,
            &m_relayClusterClient);
    }

    ~UDPHolePunchingConnectionInitiationFsm()
    {
        m_connectSessionFsm->pleaseStopSync();
    }

protected:
    void givenStartedCloudConnectSession()
    {
        using namespace std::placeholders;

        m_connectingPeerConnection = std::make_shared<TestServerConnection>();

        api::ConnectRequest request;
        request.connectSessionId = m_connectSessionId;
        request.connectionMethods = api::ConnectionMethod::all;
        m_connectSessionFsm->onConnectRequest(
            m_connectingPeerConnection,
            std::move(request),
            std::bind(&UDPHolePunchingConnectionInitiationFsm::sendConnectResponse, this, _1, _2));
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
        givenStartedCloudConnectSession();

        api::ConnectionAckRequest request;
        request.connectionMethods = api::ConnectionMethod::all;
        request.udpEndpointList.push_back(SocketAddress(HostAddress::localhost, 12345)); //< Just any port.
        m_connectSessionFsm->onConnectionAckRequest(
            std::make_shared<TestServerConnection>(),
            std::move(request),
            [](api::ResultCode) {});
    }

    void thenConnectSessionFsmShouldBeTerminatedProperly()
    {
        m_fsmFinishedPromise.get_future().wait();
    }

    void thenFullPeerNameHasBeenReportedInResponse()
    {
        auto response = m_connectResponseQueue.pop();
        ASSERT_EQ(api::ResultCode::ok, std::get<0>(response));
        ASSERT_EQ(m_listeningPeerFullName, std::get<1>(response).destinationHostFullName);
    }

private:
    std::unique_ptr<hpm::UDPHolePunchingConnectionInitiationFsm> m_connectSessionFsm;
    std::shared_ptr<TestServerConnection> m_listeningPeerConnection;
    std::shared_ptr<TestServerConnection> m_connectingPeerConnection;
    conf::Settings m_settings;
    RelayClusterClient m_relayClusterClient;
    QByteArray m_connectSessionId;
    nx::utils::promise<void> m_fsmFinishedPromise;
    nx::utils::SyncQueue<std::tuple<api::ResultCode, api::ConnectResponse>> m_connectResponseQueue;
    QString m_listeningPeerFullName;

    void connectFsmFinished(api::NatTraversalResultCode /*resultCode*/)
    {
        m_fsmFinishedPromise.set_value();
    }

    void sendConnectResponse(api::ResultCode result, api::ConnectResponse response)
    {
        m_connectResponseQueue.push(std::make_tuple(result,std::move(response)));
    }
};

TEST_F(
    UDPHolePunchingConnectionInitiationFsm,
    client_sends_connection_result_report_before_receiving_connect_response)
{
    givenStartedCloudConnectSession();
    whenClientSendsResultReport();
    thenConnectSessionFsmShouldBeTerminatedProperly();
}

TEST_F(UDPHolePunchingConnectionInitiationFsm, remote_peer_full_name_is_reported)
{
    whenRequestConnectionToThePeerUsingDomainName();
    thenFullPeerNameHasBeenReportedInResponse();
}

} // namespace test
} // namespace hpm
} // namespace nx
