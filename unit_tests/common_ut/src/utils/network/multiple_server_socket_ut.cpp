#include <gtest/gtest.h>

#include <nx/network/multiple_server_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>

#include "simple_socket_test_helper.h"

namespace nx {
namespace network {
namespace test {

static std::unique_ptr<AbstractStreamServerSocket> makeTcpUdtServerSocket()
{
    return MultipleServerSocket::make<TCPServerSocket, UdtStreamServerSocket>();
}

static std::atomic<int> makeTcpOrUdtClientSocketId(0);
static std::unique_ptr<AbstractStreamSocket> makeTcpOrUdtClientSocket()
{
    if (++makeTcpOrUdtClientSocketId % 2)
        return std::make_unique<TCPSocket>();
    else
        return std::make_unique<UdtStreamSocket>();
}

TEST(MultipleServerSocket, TcpUdtSimpleSync)
{
    socketSimpleSync(&makeTcpUdtServerSocket, &makeTcpOrUdtClientSocket);
}

TEST(MultipleServerSocket, TcpUdtSimpleAsync)
{
    socketSimpleAsync(&makeTcpUdtServerSocket, &makeTcpOrUdtClientSocket);
}

TEST(MultipleServerSocket, TcpUdtSimpleTrueAsync)
{
    socketSimpleTrueAsync(&makeTcpUdtServerSocket, &makeTcpOrUdtClientSocket);
}

} // namespace test
} // namespace network
} // namespace nx
