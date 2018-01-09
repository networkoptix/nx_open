#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/buffered_stream_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

namespace nx {
namespace network {
namespace test {

NX_NETWORK_CLIENT_SOCKET_TEST_CASE(
    TEST, BufferedStreamSocket,
    []() { return std::make_unique<TCPServerSocket>(AF_INET); },
    []()
    {
        return std::make_unique<BufferedStreamSocket>(
            std::make_unique<TCPSocket>(AF_INET));
    })

class BufferedStreamSocketTest:
    public ::testing::Test
{
protected:
    std::unique_ptr<AbstractStreamServerSocket> server;
    std::unique_ptr<AbstractStreamSocket> client;
    std::unique_ptr<BufferedStreamSocket> accepted;

    nx::utils::TestSyncQueue<SystemError::ErrorCode> acceptedResults;
    Buffer buffer;

    void SetUp() override
    {
        server = std::make_unique<TCPServerSocket>(AF_INET);
        ASSERT_TRUE(server->setReuseAddrFlag(true));
        ASSERT_TRUE(server->bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_TRUE(server->listen(10));;

        const auto serverAddress = server->getLocalAddress();
        NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);

        client = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(client->setSendTimeout(500));
        ASSERT_TRUE(client->connect(serverAddress, nx::network::kNoTimeout));

        std::unique_ptr<AbstractStreamSocket> acceptedRaw(server->accept());
        ASSERT_NE(acceptedRaw, nullptr) << SystemError::getLastOSErrorText().toStdString();

        accepted = std::make_unique<BufferedStreamSocket>(std::move(acceptedRaw));
        ASSERT_TRUE(accepted->setRecvTimeout(500));
    }
};

TEST_F(BufferedStreamSocketTest, catchRecvEvent)
{
    ASSERT_TRUE(accepted->setNonBlockingMode(true));
    accepted->catchRecvEvent(acceptedResults.pusher());
    ASSERT_EQ(SystemError::timedOut, acceptedResults.pop());

    const auto clientCount = testClientCount();
    buffer.reserve(kTestMessage.size() * clientCount);
    accepted->catchRecvEvent(acceptedResults.pusher());
    for (size_t i = 0; i < clientCount; ++i)
        ASSERT_EQ(kTestMessage.size(), client->send(kTestMessage.data(), kTestMessage.size()));

    ASSERT_EQ(SystemError::noError, acceptedResults.pop());
    accepted->readAsyncAtLeast(
        &buffer, kTestMessage.size() * clientCount,
        [&](SystemError::ErrorCode code, size_t size)
        {
            ASSERT_EQ(SystemError::noError, code);
            ASSERT_EQ(kTestMessage.size() * clientCount, size);
            ASSERT_EQ(size, (size_t)buffer.size());
            ASSERT_TRUE(buffer.startsWith(kTestMessage));
            ASSERT_TRUE(buffer.endsWith(kTestMessage));
            acceptedResults.push(code);
        });
    ASSERT_EQ(SystemError::noError, acceptedResults.pop());

    buffer = Buffer(kTestMessage.size() * clientCount, '\0');
    accepted->catchRecvEvent(acceptedResults.pusher());
    for (size_t i = 0; i < clientCount; ++i)
        ASSERT_EQ(client->send(kTestMessage.data(), kTestMessage.size()), kTestMessage.size());

    ASSERT_EQ(SystemError::noError, acceptedResults.pop());
    ASSERT_TRUE(accepted->setNonBlockingMode(false));
    ASSERT_EQ(buffer.size(), accepted->recv(buffer.data(), buffer.size(), MSG_WAITALL));
    ASSERT_TRUE(buffer.startsWith(kTestMessage));
    ASSERT_TRUE(buffer.endsWith(kTestMessage));

    ASSERT_TRUE(accepted->setNonBlockingMode(true));
    accepted->catchRecvEvent(acceptedResults.pusher());
    client.reset();
    ASSERT_EQ(acceptedResults.pop(), SystemError::connectionReset);
    ASSERT_TRUE(acceptedResults.isEmpty());
}

TEST_F(BufferedStreamSocketTest, injectRecvData)
{
    accepted->injectRecvData(kTestMessage);
    ASSERT_EQ(client->send(kTestMessage.data(), kTestMessage.size()), kTestMessage.size());

    buffer = Buffer(kTestMessage.size() * 2, '\0');
    ASSERT_EQ(accepted->recv(buffer.data(), buffer.size(), MSG_WAITALL), buffer.size());
    ASSERT_TRUE(buffer.startsWith(kTestMessage));
    ASSERT_TRUE(buffer.endsWith(kTestMessage));

    accepted->injectRecvData(kTestMessage);
    accepted->injectRecvData(kTestMessage, BufferedStreamSocket::Inject::replace);
    accepted->injectRecvData(kTestMessage, BufferedStreamSocket::Inject::begin);
    client.reset();

    buffer = Buffer(kTestMessage.size() * 3, '\0');
    ASSERT_EQ(accepted->recv(buffer.data(), buffer.size(), MSG_WAITALL), kTestMessage.size() * 2);
}

} // namespace test
} // namespace network
} // namespace nx
