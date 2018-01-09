/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/socket.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>

#include <test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, udp_transport)
{
    //TODO #ak adding multiple server to a system, checking that each very server is found

    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto system1 = addRandomSystem();
    auto system1Servers = addRandomServers(system1, 2);

    nx::network::stun::MessageParser messageParser;
    nx::network::stun::MessageSerializer messageSerializer;
    const auto udpSocket = std::make_unique<network::UDPSocket>();
    udpSocket->setRecvTimeout(3000);

    //for (int j = 0; j < 1000; ++j)
    for (size_t i = 0; i < system1Servers.size(); ++i)
    {
        //sending resolve request
        api::ResolvePeerRequest request(system1Servers[i]->serverId() + "." + system1.id);
        nx::network::stun::Message requestMessage(
            nx::network::stun::Header(
                nx::network::stun::MessageClass::request,
                nx::network::stun::extension::methods::resolvePeer));
        request.serialize(&requestMessage);
        messageSerializer.setMessage(&requestMessage);
        nx::Buffer sendBuffer;
        sendBuffer.reserve(1);
        size_t bytesWritten = 0;
        ASSERT_EQ(nx::network::server::SerializerState::done, messageSerializer.serialize(&sendBuffer, &bytesWritten));
        ASSERT_EQ(bytesWritten, (size_t)sendBuffer.size());
        ASSERT_TRUE(udpSocket->sendTo(sendBuffer.data(), sendBuffer.size(), stunEndpoint()));

        //reading response
        nx::Buffer recvBuffer;
        recvBuffer.resize(nx::network::kMaxUDPDatagramSize);
        nx::network::SocketAddress serverAddress;
        const int bytesRead = udpSocket->recvFrom(recvBuffer.data(), recvBuffer.size(), &serverAddress);
        ASSERT_NE(-1, bytesRead);
        recvBuffer.resize(bytesRead);
        nx::network::stun::Message responseMessage;
        messageParser.setMessage(&responseMessage);
        size_t bytesParsed = 0;
        ASSERT_EQ(nx::network::server::ParserState::done, messageParser.parse(recvBuffer, &bytesParsed));
        api::ResolvePeerResponse responseData;
        ASSERT_TRUE(responseData.parse(responseMessage));

        //checking response
        ASSERT_EQ(1U, responseData.endpoints.size());
        ASSERT_EQ(
            system1Servers[i]->endpoint().toString(),
            responseData.endpoints.front().toString());
    }
}

} // namespace test
} // namespace hpm
} // namespace nx
