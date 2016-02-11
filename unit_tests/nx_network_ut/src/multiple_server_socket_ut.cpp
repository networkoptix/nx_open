#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>


namespace nx {
namespace network {
namespace test {

template<quint16 kShift>
class MultipleServerSocketTester
    : public MultipleServerSocket
{
public:
    MultipleServerSocketTester()
    {
        for (int i = 0; i < kShift; ++i)
            addSocket(std::make_unique<TCPServerSocket>());
    }

    bool bind(const SocketAddress& localAddress) override
    {
        auto port = localAddress.port;
        for(auto& socket : m_serverSockets)
        {
            SocketAddress modifiedAddress(localAddress.address, port++);
            NX_LOGX(lm("bind %1 to %2")
                    .arg(modifiedAddress.toString()).arg(&socket), cl_logDEBUG1);
            if (!socket->bind(modifiedAddress))
                return false;
        }

        return true;
    }
};

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST, MultipleServerSocket,
    &std::make_unique<MultipleServerSocketTester<4>>,
    &std::make_unique<MultipleClientSocketTester<3>>)

} // namespace test
} // namespace network
} // namespace nx
