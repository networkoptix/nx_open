#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/string.h>

#include <utils/common/guard.h>

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
    SystemError::ErrorCode /*errorCode*/,
    SocketAddress /*sourceEndpoint*/,
    size_t /*bytesRead*/)
{
    ctx->readPromise.set_value();
}

} // namespace

TEST(UdpSocket, Simple)
{
    static const Buffer kTestMessage = QnUuid::createUuid().toSimpleString().toUtf8();

    UDPSocket sender(AF_INET);
    const auto senderCleanupGuard = makeScopedGuard([&sender]() { sender.pleaseStopSync(); });

    ASSERT_TRUE(sender.bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(sender.setSendTimeout(1000));

    SocketAddress senderEndpoint("127.0.0.1", sender.getLocalAddress().port);
    ASSERT_FALSE(senderEndpoint.address.isIpAddress());

    UDPSocket receiver(AF_INET);
    ASSERT_TRUE(receiver.bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(receiver.setRecvTimeout(1000));

    SocketAddress receiverEndpoint("127.0.0.1", receiver.getLocalAddress().port);
    ASSERT_FALSE(receiverEndpoint.address.isIpAddress());

    nx::utils::promise<void> sendPromise;
    ASSERT_TRUE(sender.setNonBlockingMode(true));
    sender.sendToAsync(
        kTestMessage, receiverEndpoint,
        [&](SystemError::ErrorCode code, SocketAddress ip, size_t size)
        {
            ASSERT_EQ(SystemError::noError, code);
            ASSERT_TRUE(ip.address.isIpAddress());
            ASSERT_EQ(receiverEndpoint.toString(), ip.toString());
            ASSERT_EQ(kTestMessage.size(), size);
            sendPromise.set_value();
        });

    Buffer buffer;
    buffer.resize(1024);
    SocketAddress remoteEndpoint;
    ASSERT_EQ(kTestMessage.size(), receiver.recvFrom(buffer.data(), buffer.size(), &remoteEndpoint));
    ASSERT_EQ(kTestMessage, buffer.left(kTestMessage.size()));
    ASSERT_TRUE(remoteEndpoint.address.isIpAddress());
    ASSERT_EQ(senderEndpoint.toString(), remoteEndpoint.toString());

    sendPromise.get_future().wait();
}

TEST(UdpSocket, DISABLED_multipleSocketsOnTheSamePort)
{
    constexpr int socketCount = 2;

    std::vector<SocketContext> sockets;
    const auto socketsCleanupGuard = makeScopedGuard(
        [&sockets]()
        {
            for (auto& ctx : sockets)
                ctx.socket->pleaseStopSync();
        });

    for (std::size_t i = 0; i < socketCount; ++i)
    {
        sockets.push_back(SocketContext());

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

TEST(UdpSocket, DISABLED_Performance)
{
    const uint64_t kBufferSize = 1500;
    const uint64_t kTransferSize = uint64_t(10) * 1024 * 1024 * 1024;

    UDPSocket server(AF_INET);
    server.bind(SocketAddress::anyPrivateAddress);
    const auto address = server.getLocalAddress();
    NX_LOG(lm("%1").str(address), cl_logINFO);
    std::thread serverThread(
        [&]()
        {
            const auto startTime = std::chrono::steady_clock::now();
            Buffer buffer((int) kBufferSize, Qt::Uninitialized);

            int recv = 0;
            uint64_t transferSize = 0;
            uint64_t transferCount = 0;
            while (transferSize < kTransferSize)
            {
                recv = server.recv(buffer.data(), buffer.size(), 0);
                if (recv <= 0)
                    break;

                transferSize += (uint64_t) recv;
                transferCount += 1;
            }

            const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);

            NX_LOG(lm("Resieve ended (%1): %2")
                .strs(recv, SystemError::getLastOSErrorText()), cl_logINFO);

            const auto bytesPerS = double(transferSize) * 1000 / durationMs.count();
            NX_LOG(lm("Resieved size=%1b, count=%2, average=%3, duration=%4, speed=%5bps")
                .strs(nx::utils::bytesToString(transferSize), transferCount,
                    nx::utils::bytesToString(transferSize / transferCount),
                    durationMs, nx::utils::bytesToString((uint64_t) bytesPerS)), cl_logINFO);
        });

    std::thread clientThread(
        [&]()
        {
            UDPSocket client(AF_INET);
            client.setDestAddr(address);
            Buffer buffer((int) kBufferSize, 'X');
            int send = 0;
            uint64_t transferSize = 0;
            while (transferSize < kTransferSize + kTransferSize / 10)
            {
                send = client.send(buffer.data(), buffer.size());
                if (send <= 0)
                    break;

                transferSize += (uint64_t) send;
            }
        });

    serverThread.join();
    clientThread.join();
}

} // namespace test
} // namespace network
} // namespace nx
