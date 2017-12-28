#include <chrono>
#include <thread>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class FtMediatorProcess:
    public MediatorFunctionalTest
{
public:
    FtMediatorProcess()
    {
        startMediator();
    }

    ~FtMediatorProcess()
    {
        if (m_udpClient)
        {
            m_udpClient->pleaseStopSync();
            m_udpClient.reset();
        }

        stopMediator();
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

    void stopMediator()
    {
        stop();
    }

protected:
    void startIssuingMediatorRequests()
    {
        m_sendRequestsNonStop = true;
        issueConnectRequest();
    }

    void whenSeveralJunkPacketsHaveBeenSentToMediatorUdpPort()
    {
        constexpr int kRequestToSendCount = 17;
        constexpr int kMaxPacketSize = 2*1024;

        nx::network::UDPSocket udpSocket(AF_INET);
        ASSERT_TRUE(udpSocket.connect(stunEndpoint(), nx::network::kNoTimeout));
        for (int i = 0; i < kRequestToSendCount; ++i)
        {
            nx::Buffer packet = nx::utils::random::generate(kMaxPacketSize);
            ASSERT_EQ(packet.size(), udpSocket.send(packet.constData(), packet.size()))
                << SystemError::getLastOSErrorText().toStdString();
        }
    }

    void whenSendConnectRequest()
    {
        issueConnectRequest();
    }

    void thenMediatorStillAbleToProcessCorrectRequests()
    {
        whenSendConnectRequest();
        thenResponseIsReceived();
    }

    void thenResponseIsReceived()
    {
        m_prevResponse = m_receivedResponses.pop();
        ASSERT_NE(api::ResultCode::networkError, m_prevResponse.resultCode);
    }

private:
    struct Result
    {
        stun::TransportHeader transportHeader;
        api::ResultCode resultCode = api::ResultCode::networkError;
        api::ConnectResponse response;
    };

    AbstractCloudDataProvider::System m_system;
    std::unique_ptr<MediaServerEmulator> m_server;
    std::unique_ptr<api::MediatorClientTcpConnection> m_client;
    std::unique_ptr<api::MediatorClientUdpConnection> m_udpClient;
    Result m_prevResponse;
    nx::utils::SyncQueue<Result> m_receivedResponses;
    bool m_sendRequestsNonStop = false;

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
        stun::TransportHeader transportHeader,
        api::ResultCode resultCode,
        api::ConnectResponse response)
    {
        Result requestResult;
        requestResult.transportHeader = std::move(transportHeader);
        requestResult.resultCode = resultCode;
        requestResult.response = std::move(response);
        m_receivedResponses.push(std::move(requestResult));

        if (m_sendRequestsNonStop)
            issueConnectRequest();
    }
};

TEST_F(FtMediatorProcess, correct_manager_destruction_order)
{
    startIssuingMediatorRequests();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(nx::utils::random::number<int>(0, 1000)));
    stopMediator();
}

TEST_F(FtMediatorProcess, properly_handles_requests_after_receiving_junk_on_input)
{
    whenSeveralJunkPacketsHaveBeenSentToMediatorUdpPort();
    thenMediatorStillAbleToProcessCorrectRequests();
}

} // namespace test
} // namespace hpm
} // namespace nx
