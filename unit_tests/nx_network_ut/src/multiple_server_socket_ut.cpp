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
        std::unique_ptr<AbstractStreamSocket> acceptedSocket)
    {
        if (acceptedSocket)
            m_acceptedSockets.push(std::move(acceptedSocket));
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
    PerformanceMultipleServerSocket():
        m_socketsAccepted(0)
    {
    }

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

        const seconds testDurationLimit(3);
        // Limiting number of connections to limit number of allocated ports.
        const int maxConnectionsToEstablish = 2000;

        TestResult testResult;

        bool terminated = false;
        std::atomic<bool> connectorDone(false);
        nx::utils::thread establishConnectionThread(
            [this, &terminated, testDurationLimit, maxConnectionsToEstablish, &connectorDone]()
            {
                for (int i = 0; !terminated; ++i)
                {
                    TCPSocket socket;
                    ASSERT_TRUE(socket.setReuseAddrFlag(true));
                    socket.connect(m_localServerAddress, 50);
                    if (i >= maxConnectionsToEstablish && !connectorDone)
                        connectorDone = true;
                }
            });

        runAcceptLoop(testDurationLimit, &connectorDone);

        terminated = true;
        establishConnectionThread.join();

        testResult.duration = m_testRunDuration;
        testResult.totalConnectionsAccepted = m_socketsAccepted;
        testResult.connectionsAcceptedPerSecond =
            (m_socketsAccepted * millisInSecond) /
            (m_testRunDuration.count());

        return testResult;
    }

    void printResults(const TestResult& testResult)
    {
        std::cout << "Test run for " << testResult.duration.count() << " ms, "
            "accepted " << testResult.totalConnectionsAccepted << " connections, "
            "speed " << testResult.connectionsAcceptedPerSecond << " connections per second"
            << std::endl;
    }

protected:
    int m_socketsAccepted;
    std::chrono::milliseconds m_testRunDuration;

    virtual void runAcceptLoop(
        std::chrono::milliseconds testDurationLimit,
        std::atomic<bool>* isCancelled)
    {
        using namespace std::chrono;

        const auto startTime = system_clock::now();
        while ((system_clock::now() < (startTime + testDurationLimit)) && !*isCancelled)
        {
            auto clientSocket = m_serverSocket->accept();
            if (clientSocket)
                ++m_socketsAccepted;
            delete clientSocket;
        }
        const auto endTime = system_clock::now();
        m_testRunDuration = duration_cast<milliseconds>(endTime - startTime);
    }

    const std::unique_ptr<AbstractStreamServerSocket>& serverSocket()
    {
        return m_serverSocket;
    }

private:
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    SocketAddress m_localServerAddress;

    std::unique_ptr<TCPServerSocket> initializeTcpServerSocket()
    {
        auto tcpServerSocket = std::make_unique<TCPServerSocket>(AF_INET);
        NX_GTEST_ASSERT_TRUE(tcpServerSocket->bind(SocketAddress(HostAddress::localhost, 0)));
        NX_GTEST_ASSERT_TRUE(tcpServerSocket->listen(256));
        NX_GTEST_ASSERT_TRUE(tcpServerSocket->setNonBlockingMode(true));
        m_localServerAddress = tcpServerSocket->getLocalAddress();
        return tcpServerSocket;
    }
};

TEST_F(PerformanceMultipleServerSocket, single_tcp_socket)
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

//-------------------------------------------------------------------------------------------------
// PerformanceRegularTcpServerSocketSyncModeEmulation

class PerformanceRegularTcpServerSocketSyncModeEmulation:
    public PerformanceMultipleServerSocket
{
protected:
    virtual void runAcceptLoop(
        std::chrono::milliseconds testDurationLimit,
        std::atomic<bool>* isCancelled) override
    {
        using namespace std::chrono;

        const auto startTime = system_clock::now();
        while ((system_clock::now() < (startTime + testDurationLimit)) && !*isCancelled)
        {
            nx::utils::promise<std::unique_ptr<AbstractStreamSocket>> accepted;
            serverSocket()->acceptAsync(
                [this, &accepted](
                    SystemError::ErrorCode /*sysErrorCode*/,
                    std::unique_ptr<AbstractStreamSocket> acceptedConnection)
                {
                    accepted.set_value(std::move(acceptedConnection));
                });
            auto clientSocket = accepted.get_future().get();

            if (clientSocket)
                ++m_socketsAccepted;
        }
        const auto endTime = system_clock::now();
        m_testRunDuration = duration_cast<milliseconds>(endTime - startTime);
    }
};

TEST_F(PerformanceRegularTcpServerSocketSyncModeEmulation, regular_tcp_server_socket)
{
    givenTcpServerSocket();
    const auto testResult = measurePerformance();
    printResults(testResult);
}

//-------------------------------------------------------------------------------------------------
// PerformanceMultipleServerSocketAsyncAccept

class PerformanceMultipleServerSocketAsyncAccept:
    public PerformanceMultipleServerSocket
{
public:
    PerformanceMultipleServerSocketAsyncAccept():
        m_isCancelled(nullptr),
        m_testDurationLimit(0)
    {
    }

protected:
    void runAcceptLoop(
        std::chrono::milliseconds testDurationLimit,
        std::atomic<bool>* isCancelled) override
    {
        using namespace std::chrono;
        using namespace std::placeholders;

        m_startTime = steady_clock::now();
        m_isCancelled = isCancelled;
        m_testDurationLimit = testDurationLimit;
        serverSocket()->acceptAsync(
            std::bind(&PerformanceMultipleServerSocketAsyncAccept::onConnectionAccepted, this, _1, _2));

        m_testDone.get_future().wait();
    }

private:
    std::atomic<bool>* m_isCancelled;
    std::chrono::milliseconds m_testDurationLimit;
    std::chrono::steady_clock::time_point m_startTime;
    nx::utils::promise<void> m_testDone;

    void onConnectionAccepted(
        SystemError::ErrorCode /*sysErrorCode*/,
        std::unique_ptr<AbstractStreamSocket> clientSocket)
    {
        using namespace std::chrono;
        using namespace std::placeholders;

        if ((steady_clock::now() >= (m_startTime + m_testDurationLimit)) ||
            *m_isCancelled)
        {
            m_testRunDuration = duration_cast<milliseconds>(steady_clock::now() - m_startTime);
            m_testDone.set_value();
            return;
        }

        if (clientSocket)
            ++m_socketsAccepted;
        clientSocket.reset();

        serverSocket()->acceptAsync(
            std::bind(&PerformanceMultipleServerSocketAsyncAccept::onConnectionAccepted, this, _1, _2));
    }
};

TEST_F(PerformanceMultipleServerSocketAsyncAccept, single_tcp_socket)
{
    givenMultipleServerSocketWithATcpServerSocket();
    const auto testResult = measurePerformance();
    printResults(testResult);
}

TEST_F(PerformanceMultipleServerSocketAsyncAccept, regular_tcp_server_socket)
{
    givenTcpServerSocket();
    const auto testResult = measurePerformance();
    printResults(testResult);
}

} // namespace test
} // namespace network
} // namespace nx
