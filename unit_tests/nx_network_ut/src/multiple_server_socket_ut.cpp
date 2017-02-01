#include <gtest/gtest.h>

#include <memory>

#include <nx/network/multiple_server_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace network {
namespace test {

class MultipleServerSocketTester: 
    public network::MultipleServerSocket
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

class MultipleServerSocket:
    public ::testing::Test
{
public:
    ~MultipleServerSocket()
    {
        m_multipleServerSocket.pleaseStopSync();
    }

protected:
    static AddressBinder addressBinder;

    void addRegularTcpServerSocket()
    {
        auto serverSocket = std::make_unique<TCPServerSocket>(AF_INET);
        ASSERT_TRUE(serverSocket->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(serverSocket->listen())
            << SystemError::getLastOSErrorText().toStdString();
        m_aggregatedSocketsAddresses.push_back(serverSocket->getLocalAddress());
        ASSERT_TRUE(m_multipleServerSocket.addSocket(std::move(serverSocket)));
    }

    void startAccepting()
    {
        using namespace std::placeholders;

        m_multipleServerSocket.acceptAsync(
            std::bind(&MultipleServerSocket::onSocketAccepted, this, _1, _2));
    }

    void triggerServerSocketByIndex(std::size_t index)
    {
        TCPSocket socket(AF_INET);
        ASSERT_TRUE(socket.connect(m_aggregatedSocketsAddresses[index]));
    }

    void assertIfConnectionHasNotBeenAccepted()
    {
        auto acceptedSocket = m_acceptedSockets.pop();
        ASSERT_NE(nullptr, acceptedSocket);
    }

private:
    network::MultipleServerSocket m_multipleServerSocket;
    std::vector<SocketAddress> m_aggregatedSocketsAddresses;
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;

    void onSocketAccepted(
        SystemError::ErrorCode /*sysErrorCode*/,
        AbstractStreamSocket* acceptedSocket)
    {
        if (acceptedSocket)
            m_acceptedSockets.push(std::unique_ptr<AbstractStreamSocket>(acceptedSocket));
    }
};

AddressBinder MultipleServerSocket::addressBinder;

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, MultipleServerSocket,
    [](){ return std::make_unique<MultipleServerSocketTester>(&addressBinder, 5); },
    [](){ return std::make_unique<MultipleClientSocketTester>(&addressBinder); })

TEST_F(MultipleServerSocket, add_remove)
{
    network::MultipleServerSocket sock;

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

TEST_F(MultipleServerSocket, existing_sockets_are_handled_properly_after_adding_a_new_one)
{
    addRegularTcpServerSocket();
    startAccepting();
    addRegularTcpServerSocket();
    triggerServerSocketByIndex(0);
    assertIfConnectionHasNotBeenAccepted();
}

//-------------------------------------------------------------------------------------------------
// MultipleServerSocket performance tests

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

        auto sock = std::make_unique<network::MultipleServerSocket>();
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

        const auto millisInSecond = duration_cast<milliseconds>(seconds(1)).count();

        const seconds testDurationLimit(5);
        // Limiting number of connections to limit number of allocated ports.
        const int maxConnectionsToEstablish = 1000;

        TestResult testResult;

        bool terminated = false;
        bool connectorDone = false;
        nx::utils::thread establishConnectionThread(
            [this, &terminated, testDurationLimit, maxConnectionsToEstablish, &connectorDone]()
            {
                for (int i = 0; !terminated; ++i)
                {
                    TCPSocket socket;
                    ASSERT_TRUE(socket.setReuseAddrFlag(true));
                    socket.connect(m_localServerAddress, 500);
                    if (i >= maxConnectionsToEstablish && !connectorDone)
                        connectorDone = true;
                }
            });

        int socketsAccepted = 0;
        const auto startTime = system_clock::now();
        while ((system_clock::now() < (startTime + testDurationLimit)) && !connectorDone)
        {
            auto clientSocket = m_serverSocket->accept();
            if (clientSocket)
                ++socketsAccepted;
            delete clientSocket;
        }
        const auto endTime = system_clock::now();

        terminated = true;
        establishConnectionThread.join();

        testResult.duration = duration_cast<milliseconds>(endTime - startTime);
        testResult.totalConnectionsAccepted = socketsAccepted;
        testResult.connectionsAcceptedPerSecond =
            (socketsAccepted * millisInSecond) /
            (duration_cast<milliseconds>(endTime - startTime).count());

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

TEST_F(PerformanceMultipleServerSocket, regular_tcp_server_socket)
{
    givenTcpServerSocket();
    const auto testResult = measurePerformance();
    printResults(testResult);
}

} // namespace test
} // namespace network
} // namespace nx
