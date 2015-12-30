#include <gtest/gtest.h>

#include <nx/network/multiple_server_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>

#include "simple_socket_test_helper.h"

namespace nx {
namespace network {
namespace test {

class MultipleServerSocketTester
    : public MultipleServerSocket
{
public:
    MultipleServerSocketTester()
    {
        addSocket(std::make_unique<TCPServerSocket>());
        addSocket(std::make_unique<TCPServerSocket>());
    }

    bool bind(const SocketAddress& localAddress) override
    {
        auto port = localAddress.port;
        for(auto& socket : m_serverSockets)
            if (!socket->bind(SocketAddress(localAddress.address, port++)))
                return false;

        return true;
    }
};

TEST(MultipleServerSocket, TcpSimpleSync)
{
    socketSimpleSync(&std::make_unique<MultipleServerSocketTester>,
                     &std::make_unique<TCPSocket>);
}

TEST(MultipleServerSocket, TcpSimpleAsync)
{
    socketSimpleAsync(&std::make_unique<MultipleServerSocketTester>,
                      &std::make_unique<TCPSocket>);
}

TEST(MultipleServerSocket, TcpSimpleTrueAsync)
{
    socketSimpleTrueAsync(&std::make_unique<MultipleServerSocketTester>,
                          &std::make_unique<TCPSocket>);
}

} // namespace test
} // namespace network
} // namespace nx
