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

TEST(MultipleServerSocket, add_remove)
{
    MultipleServerSocket sock;

    auto tcpServerSocket = std::make_unique<TCPServerSocket>();
    ASSERT_TRUE(tcpServerSocket->bind(SocketAddress(HostAddress::localhost, 0)));
    ASSERT_TRUE(tcpServerSocket->listen());
    sock.addSocket(std::move(tcpServerSocket));

    ConnectionsGenerator connectionGenerator(
        SocketAddress(HostAddress::localhost, 1234),
        100,
        test::TestTrafficLimitType::none,
        1024*1024,
        10000,
        TestTransmissionMode::spam);
    connectionGenerator.start();

    for (int i = 0; i < 100; ++i)
    {
        auto udtServerSocket = std::make_unique<TCPServerSocket>();
        ASSERT_TRUE(udtServerSocket->bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_TRUE(udtServerSocket->listen());

        connectionGenerator.setRemoteAddress(udtServerSocket->getLocalAddress());

        sock.addSocket(std::move(udtServerSocket));
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
        sock.removeSocket(1);
    }

    connectionGenerator.pleaseStopSync();
}

} // namespace test
} // namespace network
} // namespace nx
