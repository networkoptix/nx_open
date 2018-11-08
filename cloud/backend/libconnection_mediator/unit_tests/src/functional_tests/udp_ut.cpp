#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/socket.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/network/stun/udp_client.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/mediator/test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class UdpApi:
    public MediatorFunctionalTest
{
public:
    virtual ~UdpApi()
    {
        if (m_udpClient)
            m_udpClient->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_udpClient = std::make_unique<nx::network::stun::UdpClient>(stunUdpEndpoint());
    }

    void sendResolvePeerRequest(const nx::String& hostName)
    {
        using namespace std::placeholders;
        using namespace nx::hpm;

        api::ResolvePeerRequest request(hostName);
        nx::network::stun::Message requestMessage(
            nx::network::stun::Header(
                nx::network::stun::MessageClass::request,
                nx::network::stun::extension::methods::resolvePeer));
        request.serialize(&requestMessage);

        m_udpClient->sendRequest(
            requestMessage,
            std::bind(&UdpApi::saveResponse, this, _1, _2));
    }

    void assertValidResponseIsReceived(
        const nx::network::SocketAddress& serverEndpoint)
    {
        const auto result = m_responseQueue.pop();

        ASSERT_EQ(SystemError::noError, std::get<0>(result))
            << SystemError::toString(std::get<0>(result)).toStdString();

        api::ResolvePeerResponse responseData;
        ASSERT_TRUE(responseData.parse(std::get<1>(result)));

        // Checking response.
        ASSERT_EQ(1U, responseData.endpoints.size());
        ASSERT_EQ(serverEndpoint.toString(), responseData.endpoints.front().toString());
    }

private:
    std::unique_ptr<nx::network::stun::UdpClient> m_udpClient;
    nx::utils::SyncQueue<
        std::tuple<SystemError::ErrorCode, nx::network::stun::Message>
    > m_responseQueue;

    void saveResponse(
        SystemError::ErrorCode resultCode,
        nx::network::stun::Message responseMessage)
    {
        m_responseQueue.push(
            std::make_tuple(resultCode, std::move(responseMessage)));
    }
};

TEST_F(UdpApi, resolve_peer_request_works)
{
    const auto system1 = addRandomSystem();
    auto system1Servers = addRandomServers(system1, 2);

    for (size_t i = 0; i < system1Servers.size(); ++i)
    {
        // Sending resolve request.
        sendResolvePeerRequest(system1Servers[i]->serverId() + "." + system1.id);
        assertValidResponseIsReceived(system1Servers[i]->endpoint());
    }
}

} // namespace test
} // namespace hpm
} // namespace nx
