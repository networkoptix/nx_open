/**********************************************************
* June 7, 2016
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/utils/std/future.h>


namespace nx {
namespace network {
namespace test {

namespace {

struct SocketContext
{
    std::unique_ptr<UDPSocket> socket;
    nx::Buffer readBuffer;
    nx::utils::promise<void> readPromise;
};

void onBytesRead(
    SocketContext* ctx,
    SystemError::ErrorCode errorCode,
    SocketAddress sourceEndpoint,
    size_t bytesRead)
{
    ctx->readPromise.set_value();
}

}

TEST(UdpSocket, DISABLED_multipleSocketsOnTheSamePort)
{
    std::vector<SocketContext> sockets;
    sockets.resize(2);
    for (std::size_t i = 0; i < sockets.size(); ++i)
    {
        sockets[i].socket = std::make_unique<UDPSocket>();
        sockets[i].readBuffer.reserve(1024);
        ASSERT_TRUE(sockets[i].socket->setReuseAddrFlag(true));
        ASSERT_TRUE(sockets[i].socket->setNonBlockingMode(true));
        ASSERT_TRUE(sockets[i].socket->setRecvTimeout(0));
        if (i == 0)
        {
            ASSERT_TRUE(sockets[i].socket->bind(SocketAddress(HostAddress::localhost, 0)));
        }
        else
        {
            ASSERT_TRUE(sockets[i].socket->bind(sockets[0].socket->getLocalAddress()));
        }

        using namespace std::placeholders;
        sockets[i].socket->recvFromAsync(
            &sockets[i].readBuffer,
            std::bind(&onBytesRead, &sockets[i], _1, _2, _3));
    }

    constexpr const char* testMessage = "bla-bla-bla";
    UDPSocket sendingSocket;
    ASSERT_TRUE(
        sendingSocket.sendTo(
            testMessage,
            strlen(testMessage),
            sockets[0].socket->getLocalAddress()));

    for (auto& ctx: sockets)
    {
        ASSERT_EQ(
            std::future_status::ready,
            ctx.readPromise.get_future().wait_for(std::chrono::seconds(1)));
        ASSERT_EQ(testMessage, ctx.readBuffer);
    }
}

}   //namespace test
}   //namespace network
}   //namespace nx
