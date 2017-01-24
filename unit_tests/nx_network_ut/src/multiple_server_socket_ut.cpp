#include <gtest/gtest.h>

#include <nx/network/multiple_server_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace network {
namespace test {

class MultipleServerSocketTester: 
    public MultipleServerSocket
{
public:
    MultipleServerSocketTester(AddressBinder* addressBinder, size_t count):
        m_addressManager(addressBinder)
    {
        for (size_t i = 0; i < count; ++i)
            addSocket(std::make_unique<TCPServerSocket>(AF_INET));
    }

    ~MultipleServerSocketTester()
    {
        m_addressManager.wipe();
    }

    virtual bool bind(const SocketAddress& localAddress) override
    {
        for(auto& socket : m_serverSockets)
        {
            if (!socket->bind(SocketAddress::anyPrivateAddress))
                return false;

            m_addressManager.add(socket->getLocalAddress());
        }

        static_cast<void>(localAddress);
        return true;
    }

    virtual SocketAddress getLocalAddress() const override
    {
        return m_addressManager.key;
    }

private:
    AddressBinder::Manager m_addressManager;
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

    auto tcpServerSocket = std::make_unique<TCPServerSocket>(AF_INET);
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
        auto udtServerSocket = std::make_unique<TCPServerSocket>(AF_INET);
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

class PerformanceMultipleServerSocket:
    public ::testing::Test
{
public:
    ~PerformanceMultipleServerSocket()
    {
        m_serverSocket->pleaseStopSync();
    }

protected:
    struct TestResult
    {
        std::chrono::milliseconds duration;
        std::size_t totalConnectionsAccepted;
        std::size_t connectionsAcceptedPerSecond;

        TestResult():
            totalConnectionsAccepted(0),
            connectionsAcceptedPerSecond(0)
        {
        }
    };

    void givenMultipleServerSocketWithATcpServerSocket()
    {
        auto tcpServerSocket = initializeTcpServerSocket();

        auto sock = std::make_unique<MultipleServerSocket>();
        sock->addSocket(std::move(tcpServerSocket));
        m_serverSocket = std::move(sock);
    }

    void givenTcpServerSocket()
    {
        m_serverSocket = initializeTcpServerSocket();
    }

    TestResult measurePerformance()
    {
        using namespace std::chrono;

        const seconds testDuration(5);

        TestResult testResult;

        bool terminated = false;
        nx::utils::thread establishConnectionThread(
            [this, &terminated, testDuration]()
            {
                while (!terminated)
                {
                    TCPSocket socket;
                    socket.connect(m_localServerAddress, 50);
                }
            });

        int socketsAccepted = 0;
        const auto startTime = system_clock::now();
        while (system_clock::now() < (startTime + testDuration))
        {
            auto clientSocket = m_serverSocket->accept();
            delete clientSocket;
            ++socketsAccepted;
        }
        const auto endTime = system_clock::now();

        terminated = true;
        establishConnectionThread.join();

        testResult.duration = duration_cast<milliseconds>(endTime - startTime);
        testResult.totalConnectionsAccepted = socketsAccepted;
        testResult.connectionsAcceptedPerSecond =
            socketsAccepted / (duration_cast<seconds>(endTime - startTime).count());

        return testResult;
    }

    void printResults(const TestResult& testResult)
    {
        std::cout << "Test run for " << testResult.duration.count() << " ms, "
            "accepted " << testResult.totalConnectionsAccepted << " connections, "
            "speed " << testResult.connectionsAcceptedPerSecond << " connections per second"
            << std::endl;
    }

private:
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    SocketAddress m_localServerAddress;

    std::unique_ptr<TCPServerSocket> initializeTcpServerSocket()
    {
        auto tcpServerSocket = std::make_unique<TCPServerSocket>(AF_INET);
        NX_GTEST_ASSERT_TRUE(tcpServerSocket->bind(SocketAddress(HostAddress::localhost, 0)));
        NX_GTEST_ASSERT_TRUE(tcpServerSocket->listen());
        m_localServerAddress = tcpServerSocket->getLocalAddress();
        return tcpServerSocket;
    }
};

TEST_F(PerformanceMultipleServerSocket, single_tcp_socket_blocking_accept)
{
    givenMultipleServerSocketWithATcpServerSocket();
    const auto testResult = measurePerformance();
    printResults(testResult);
}

TEST_F(PerformanceMultipleServerSocket, just_tcp_server_socket)
{
    givenTcpServerSocket();
    const auto testResult = measurePerformance();
    printResults(testResult);
}

} // namespace test
} // namespace network
} // namespace nx
