#include "mediator_scalability_test_fixture.h"

namespace nx::hpm::test {

class StunRedirection: public MediatorScalabilityTestFixture
{
public:
    ~StunRedirection()
    {
        m_udpClient->pleaseStopSync();
    }

protected:
    void whenTryToConnectToServerOnDifferentMediator()
    {
        auto& mediator = m_mediatorCluster.mediator(1);

        m_udpClient =
            std::make_unique<api::MediatorClientUdpConnection>(mediator.stunUdpEndpoint());

        api::ConnectRequest request;
        request.connectionMethods = api::ConnectionMethod::udpHolePunching;
        request.connectSessionId = QnUuid::createUuid().toSimpleByteArray();
        request.destinationHostName = m_mediaServerFullName.c_str();
        request.originatingPeerId = QnUuid::createUuid().toSimpleByteArray();

        m_udpClient->connect(
            request,
            [this](
                nx::network::stun::TransportHeader stunTransportHeader,
                nx::hpm::api::ResultCode resultCode,
                nx::hpm::api::ConnectResponse response)
            {
                m_connectResponseQueue.push(std::make_tuple(resultCode, std::move(response)));
                m_connectionHeader = stunTransportHeader;
            });
    }

    void andConnectionToDifferentMediatorIsEstablished()
    {
        ASSERT_EQ(
            m_mediatorCluster.mediator(0).stunUdpEndpoint().toString(),
            m_connectionHeader.locationEndpoint.toString());
    }

private:
    std::unique_ptr<api::MediatorClientUdpConnection> m_udpClient;
    nx::network::stun::TransportHeader m_connectionHeader;
};

TEST_F(StunRedirection, stun_connect_request_redirected_to_correct_mediator)
{
    givenSynchronizedClusterWithListeningServer();

    whenTryToConnectToServerOnDifferentMediator();

    thenRequestIsRedirected();

    andConnectionToDifferentMediatorIsEstablished();
}

} // namespace nx::hpm::test