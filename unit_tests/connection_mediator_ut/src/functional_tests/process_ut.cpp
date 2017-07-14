#include <chrono>
#include <thread>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/utils/random.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class FtMediatorProcess:
    public MediatorFunctionalTest
{
public:
    ~FtMediatorProcess()
    {
        if (m_udpClient)
        {
            m_udpClient->pleaseStopSync();
            m_udpClient.reset();
        }
    }

    void startMediator()
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_system = addRandomSystem();
        m_server = addRandomServer(m_system);
        ASSERT_NE(nullptr, m_server);
        ASSERT_EQ(nx::hpm::api::ResultCode::ok, m_server->listen().first);

        m_client = clientConnection();

        m_udpClient = std::make_unique<api::MediatorClientUdpConnection>(stunEndpoint());
    }
    
    void startIssuingMediatorRequests()
    {
        issueConnectRequest();
    }

    void stopMediator()
    {
        stop();
    }

private:
    AbstractCloudDataProvider::System m_system;
    std::unique_ptr<MediaServerEmulator> m_server;
    std::unique_ptr<api::MediatorClientTcpConnection> m_client;
    std::unique_ptr<api::MediatorClientUdpConnection> m_udpClient;

    void issueConnectRequest()
    {
        using namespace std::placeholders;

        api::ConnectRequest connectRequest;
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.connectSessionId = QnUuid::createUuid().toSimpleByteArray();
        connectRequest.destinationHostName = m_server->fullName();
        connectRequest.originatingPeerId = QnUuid::createUuid().toSimpleByteArray();
        m_udpClient->connect(
            std::move(connectRequest),
            std::bind(&FtMediatorProcess::onConnectResponse, this, _1, _2, _3));
    }

    void onConnectResponse(
        stun::TransportHeader /*transportHeader*/,
        api::ResultCode /*resultCode*/,
        api::ConnectResponse /*response*/)
    {
        issueConnectRequest();
    }
};

TEST_F(FtMediatorProcess, correct_manager_destruction_order)
{
    startMediator();
    startIssuingMediatorRequests();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(nx::utils::random::number<int>(0, 1000)));
    stopMediator();
}

} // namespace test
} // namespace hpm
} // namespace nx
