#include <gtest/gtest.h>

#include <nx/network/multiple_server_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>

namespace nx {
namespace network {
namespace test {

class MultipleServerSocketTester
    : public MultipleServerSocket
{
public:
    MultipleServerSocketTester(AddressBinder* addressBinder, size_t count)
    :
        m_addressBinder(addressBinder)
    {
        for (size_t i = 0; i < count; ++i)
            addSocket(std::make_unique<TCPServerSocket>());
    }

    bool bind(const SocketAddress& localAddress) override
    {
        m_boundAddress = m_addressBinder->bind();
        for(auto& socket : m_serverSockets)
        {
            if (!socket->bind(SocketAddress()))
                return false;

            m_addressBinder->add(m_boundAddress, socket->getLocalAddress());
        }

        static_cast<void>(localAddress);
        return true;
    }

    SocketAddress getLocalAddress()
    {
        return m_boundAddress;
    }

private:
    AddressBinder* m_addressBinder;
    SocketAddress m_boundAddress;
};

class MultipleServerSocketTest : public ::testing::Test
{
protected:
    static AddressBinder ab;
};

AddressBinder MultipleServerSocketTest::ab;

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, MultipleServerSocketTest,
    [](){ return std::make_unique<MultipleServerSocketTester>(&ab, 5); },
    [](){ return std::make_unique<MultipleClientSocketTester>(&ab); })

TEST_F(MultipleServerSocketTest, add_remove)
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

        connectionGenerator.resetRemoteAddresses(
            {udtServerSocket->getLocalAddress()});

        sock.addSocket(std::move(udtServerSocket));
        std::this_thread::sleep_for(std::chrono::milliseconds(nx::utils::random::number(0, 100)));
        sock.removeSocket(1);
    }

    connectionGenerator.pleaseStopSync();
}

} // namespace test
} // namespace network
} // namespace nx
