/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/socket.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <utils/common/string.h>
#include <utils/common/sync_call.h>

#include <test_support/mediaserver_emulator.h>
#include <test_support/mediator_functional_test.h>


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, udp_transport)
{
    //TODO #ak adding multiple server to a system, checking that each very server is found

    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const auto system1 = addRandomSystem();
    auto system1Servers = addRandomServers(system1, 2);

    nx::stun::MessageParser messageParser;
    nx::stun::MessageSerializer messageSerializer;
    auto udpSocket = SocketFactory::createDatagramSocket();
    udpSocket->setRecvTimeout(3000);

    //for (int j = 0; j < 1000; ++j)
    for (int i = 0; i < system1Servers.size(); ++i)
    {
        //sending resolve request
        api::ResolveRequest request(system1Servers[i]->serverId() + "." + system1.id);
        nx::stun::Message requestMessage(
            stun::Header(
                nx::stun::MessageClass::request,
                nx::stun::cc::methods::resolve));
        request.serialize(&requestMessage);
        messageSerializer.setMessage(&requestMessage);
        nx::Buffer sendBuffer;
        sendBuffer.reserve(1);
        size_t bytesWritten = 0;
        ASSERT_EQ(nx_api::SerializerState::done, messageSerializer.serialize(&sendBuffer, &bytesWritten));
        ASSERT_EQ(bytesWritten, sendBuffer.size());
        ASSERT_TRUE(udpSocket->sendTo(sendBuffer, endpoint()));

        //reading response
        nx::Buffer recvBuffer;
        recvBuffer.resize(nx::network::kMaxUDPDatagramSize);
        SocketAddress serverAddress;
        const int bytesRead = udpSocket->recvFrom(recvBuffer.data(), recvBuffer.size(), &serverAddress);
        ASSERT_NE(-1, bytesRead);
        recvBuffer.resize(bytesRead);
        nx::stun::Message responseMessage;
        messageParser.setMessage(&responseMessage);
        size_t bytesParsed = 0;
        ASSERT_EQ(nx_api::ParserState::done, messageParser.parse(recvBuffer, &bytesParsed));
        api::ResolveResponse responseData;
        ASSERT_TRUE(responseData.parse(responseMessage));

        //checking response
        ASSERT_TRUE(std::find(
            responseData.endpoints.begin(),
            responseData.endpoints.end(),
            system1Servers[i]->endpoint()) != responseData.endpoints.end());
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
