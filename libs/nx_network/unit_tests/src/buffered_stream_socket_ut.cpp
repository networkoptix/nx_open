// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/buffered_stream_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

namespace nx {
namespace network {
namespace test {

class BufferedStreamSocket:
    public ::testing::Test
{
protected:
    std::unique_ptr<AbstractStreamServerSocket> server;
    std::unique_ptr<AbstractStreamSocket> client;
    std::unique_ptr<network::BufferedStreamSocket> accepted;

    nx::utils::TestSyncQueue<SystemError::ErrorCode> acceptedResults;
    Buffer buffer;

    void SetUp() override
    {
        server = std::make_unique<TCPServerSocket>(AF_INET);
        ASSERT_TRUE(server->setReuseAddrFlag(true));
        ASSERT_TRUE(server->bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_TRUE(server->listen(10));;

        const auto serverAddress = server->getLocalAddress();
        NX_DEBUG(this, nx::format("Server address: %1").arg(serverAddress.toString()));

        client = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(client->setSendTimeout(500));
        ASSERT_TRUE(client->connect(serverAddress, nx::network::kNoTimeout));

        auto acceptedRaw = server->accept();
        ASSERT_NE(acceptedRaw, nullptr) << SystemError::getLastOSErrorText();

        accepted = std::make_unique<network::BufferedStreamSocket>(
            std::move(acceptedRaw),
            nx::Buffer());
        ASSERT_TRUE(accepted->setNonBlockingMode(true));
    }
};

TEST_F(BufferedStreamSocket, catchRecvEvent_times_out)
{
    ASSERT_TRUE(accepted->setRecvTimeout(1));

    accepted->catchRecvEvent(acceptedResults.pusher());
    ASSERT_EQ(SystemError::timedOut, acceptedResults.pop());
}

TEST_F(BufferedStreamSocket, catchRecvEvent)
{
    ASSERT_TRUE(accepted->setRecvTimeout(kNoTimeout.count()));

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
            ASSERT_TRUE(buffer.starts_with(kTestMessage));
            ASSERT_TRUE(buffer.ends_with(kTestMessage));
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
    ASSERT_TRUE(buffer.starts_with(kTestMessage));
    ASSERT_TRUE(buffer.ends_with(kTestMessage));

    ASSERT_TRUE(accepted->setNonBlockingMode(true));
    accepted->catchRecvEvent(acceptedResults.pusher());
    client.reset();
    ASSERT_EQ(acceptedResults.pop(), SystemError::connectionReset);
    ASSERT_TRUE(acceptedResults.isEmpty());
}

NX_NETWORK_CLIENT_SOCKET_TEST_CASE(
    TEST_F, BufferedStreamSocket,
    []() { return std::make_unique<TCPServerSocket>(AF_INET); },
    []()
    {
        return std::make_unique<network::BufferedStreamSocket>(
            std::make_unique<TCPSocket>(AF_INET),
            nx::Buffer());
    })

} // namespace test
} // namespace network
} // namespace nx
