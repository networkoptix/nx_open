/**********************************************************
* Jan 19, 2016
* akolesnikov
***********************************************************/

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/stun/abstract_server_connection.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>

#include <server/udp_hole_punching_connection_initiation_fsm.h>
#include <settings.h>

namespace nx {
namespace hpm {
namespace test {

class TestServerConnection:
    public stun::AbstractServerConnection
{
public:
    virtual void sendMessage(
        nx::stun::Message message,
        std::function<void(SystemError::ErrorCode)> handler = nullptr) override
    {
    }

    virtual nx::network::TransportProtocol transportProtocol() const override
    {
        return nx::network::TransportProtocol::tcp;
    }

    virtual SocketAddress getSourceAddress() const override
    {
        return SocketAddress();
    }

    virtual void addOnConnectionCloseHandler(nx::utils::MoveOnlyFunc<void()> handler) override
    {
    }

    virtual AbstractCommunicatingSocket* socket() override
    {
        return &m_socket;
    }

private:
    nx::network::TCPSocket m_socket;
};

class UDPHolePunchingConnectionInitiationFsm:
    public ::testing::Test
{
public:
    UDPHolePunchingConnectionInitiationFsm()
    {
        using namespace std::placeholders;

        m_connectSessionId = nx::utils::generateRandomName(7);
        m_listeningPeerConnection = std::make_shared<TestServerConnection>();

        ListeningPeerData listeningPeerData;
        listeningPeerData.peerConnection = m_listeningPeerConnection;
        m_connectSessionFsm = std::make_unique<hpm::UDPHolePunchingConnectionInitiationFsm>(
            m_connectSessionId,
            listeningPeerData,
            std::bind(&UDPHolePunchingConnectionInitiationFsm::connectFsmFinished, this, _1),
            m_settings);
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

    void thenConnectSessionFsmShouldBeTerminatedProperly()
    {
        m_fsmFinishedPromise.get_future().wait();
    }

private:
    std::unique_ptr<hpm::UDPHolePunchingConnectionInitiationFsm> m_connectSessionFsm;
    std::shared_ptr<TestServerConnection> m_listeningPeerConnection;
    std::shared_ptr<TestServerConnection> m_connectingPeerConnection;
    conf::Settings m_settings;
    QByteArray m_connectSessionId;
    nx::utils::promise<void> m_fsmFinishedPromise;

    void connectFsmFinished(api::ResultCode /*resultCode*/)
    {
        m_fsmFinishedPromise.set_value();
    }

    void sendConnectResponse(api::ResultCode /*result*/, api::ConnectResponse /*message*/)
    {
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

} // namespace test
} // namespace hpm
} // namespace nx
