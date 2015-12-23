#include <gtest/gtest.h>

#include <nx/network/cloud_connectivity/cloud_stream_socket.h>
#include <nx/network/cloud_connectivity/cloud_server_socket.h>

#include "utils/network/simple_socket_test_helper.h"

namespace nx {
namespace cc {
namespace test {

static const SocketAddress kTestAddress("localhost:12345");
static const QByteArray kTestMessage("Ping");
static const size_t kClientCount(3);

TEST(CloudStreamSocket, SimpleSync)
{
    socketSimpleSync<TCPServerSocket, CloudStreamSocket>(
        kTestAddress, kTestMessage, kClientCount);
}

TEST(CloudStreamSocket, SimpleAsync)
{
    socketSimpleAsync<TCPServerSocket, CloudStreamSocket>(
        kTestAddress, kTestMessage, kClientCount,
        [this](std::unique_ptr<QnStoppableAsync> socket) {
            socket->pleaseStopSync();
        });
}

TEST(CloudStreamSocket, SimpleTrueAsync)
{
    socketSimpleTrueAsync<TCPServerSocket, CloudStreamSocket>(
        kTestAddress, kTestMessage, kClientCount);
}

// ---

TEST(CloudServerSocket, SimpleSync)
{
    socketSimpleSync<CloudServerSocket, TCPSocket>(
        kTestAddress, kTestMessage, kClientCount);
}

TEST(CloudServerSocket, SimpleAsync)
{
    socketSimpleAsync<CloudServerSocket, TCPSocket>(
        kTestAddress, kTestMessage, kClientCount,
        [this](std::unique_ptr<QnStoppableAsync> socket) {
            socket->pleaseStopSync();
        });
}

TEST(CloudServerSocket, SimpleTrueAsync)
{
    socketSimpleTrueAsync<CloudServerSocket, TCPSocket>(
        kTestAddress, kTestMessage, kClientCount);
}

// ---

TEST(CloudSockets, SimpleSync)
{
    socketSimpleSync<CloudServerSocket, CloudStreamSocket>(
        kTestAddress, kTestMessage, kClientCount);
}

TEST(CloudSockets, SimpleAsync)
{
    socketSimpleAsync<CloudServerSocket, CloudStreamSocket>(
        kTestAddress, kTestMessage, kClientCount,
        [this](std::unique_ptr<QnStoppableAsync> socket) {
            socket->pleaseStopSync();
        });
}

TEST(CloudSockets, SimpleTrueAsync)
{
    socketSimpleTrueAsync<CloudServerSocket, CloudStreamSocket>(
        kTestAddress, kTestMessage, kClientCount);
}

} // namespace test
} // namespace cc
} // namespace nx
