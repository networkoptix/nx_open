#pragma once

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/std/thread.h>

namespace nx {
namespace network {
namespace test {

namespace {

constexpr int kIterations = 100;
constexpr int kBufferSize = 1024 * 32;
constexpr std::chrono::milliseconds kClientDelay(1);
constexpr int kConcurrentConnections = 17;
constexpr int kTotalConnections = 77;

} // namespace

template<typename SocketTypeSet>
class StreamSocketAcceptance:
    public ::testing::Test
{
public:
    StreamSocketAcceptance():
        m_addressResolver(&nx::network::SocketGlobals::addressResolver()),
        m_startedConnectionsCount(0),
        m_completedConnectionsCount(0)
    {
    }

    ~StreamSocketAcceptance()
    {
        if (m_connection)
            m_connection->pleaseStopSync();
        if (m_serverSocket)
            m_serverSocket->pleaseStopSync();
    }

protected:
    std::mutex m_mutex;
    AddressResolver* m_addressResolver = nullptr;
    std::atomic<int> m_startedConnectionsCount;
    std::atomic<int> m_completedConnectionsCount;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_connections;

    void givenListeningServer()
    {
        m_serverSocket = std::make_unique<typename SocketTypeSet::ServerSocket>(
            SocketFactory::tcpClientIpVersion());
        ASSERT_TRUE(m_serverSocket->setNonBlockingMode(true));
        ASSERT_TRUE(m_serverSocket->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_serverSocket->listen());
    }

    void givenRandomNameMappedToServerHostIp()
    {
        givenRandomHostName();
        m_mappedEndpoint.port = m_serverSocket->getLocalAddress().port;

        m_addressResolver->addFixedAddress(
            m_mappedEndpoint.address,
            m_serverSocket->getLocalAddress());
    }

    void givenRandomHostName()
    {
        m_mappedEndpoint.address = nx::utils::random::generateName(7).toStdString();
        m_mappedEndpoint.port = nx::utils::random::number<std::uint16_t>();
    }

    void whenConnectUsingHostName()
    {
        using namespace std::placeholders;

        m_connection = std::make_unique<typename SocketTypeSet::ClientSocket>();
        ASSERT_TRUE(m_connection->setNonBlockingMode(true));
        m_connection->connectAsync(
            m_mappedEndpoint,
            std::bind(&StreamSocketAcceptance::saveConnectResult, this, _1));
    }

    void whenCancelAllSocketOperations()
    {
        m_connection->pleaseStopSync();
    }

    void thenConnetionIsEstablished()
    {
        ASSERT_EQ(SystemError::noError, m_connectResultQueue.pop());
    }

    void assertConnectionToServerCanBeEstablishedUsingMappedName()
    {
        whenConnectUsingHostName();
        thenConnetionIsEstablished();
    }

    void runStreamingTest(bool doServerDelay, bool doClientDelay)
    {
        using namespace std::chrono;

        nx::utils::thread serverThread(
            std::bind(&StreamSocketAcceptance::streamingServerMain, this, doServerDelay));

        typename SocketTypeSet::ClientSocket clientSocket;
        SocketAddress addr("127.0.0.1", m_serverPort.get_future().get());
        ASSERT_TRUE(clientSocket.connect(addr, nx::network::kNoTimeout));
        if (doServerDelay)
            clientSocket.setSendTimeout(duration_cast<milliseconds>(kClientDelay).count());

        std::vector<int> buffer(kBufferSize);
        for (int i = 0; i < kBufferSize; ++i)
            buffer[i] = i;

        for (int i = 0; i < kIterations; ++i)
        {
            const int bufferSize = kBufferSize * sizeof(int);
            int offset = 0;
            while (offset < bufferSize)
            {
                int bytesSent = clientSocket.send((char *)buffer.data() + offset, bufferSize - offset);
                if (bytesSent > 0)
                {
                    offset += bytesSent;
                }
                else
                {
                    auto error = SystemError::getLastOSErrorCode();
                    if (error != SystemError::timedOut && error != SystemError::wouldBlock)
                    {
                        ASSERT_EQ(0, error);
                        break; //< Send error.
                    }
                }
            }
            if (doClientDelay)
                std::this_thread::sleep_for(kClientDelay);
        }
        clientSocket.close();
        serverThread.join();
    }

    //---------------------------------------------------------------------------------------------
    // Stopping multiple connections test.

    void startMaximumConcurrentConnections()
    {
        m_startedConnectionsCount = kConcurrentConnections;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            for (int i = 0; i < kConcurrentConnections; ++i)
                startAnotherSocketNonSafe();
        }
    }

    void startAnotherSocketNonSafe()
    {
        using namespace std::placeholders;

        auto connection = std::make_unique<typename SocketTypeSet::ClientSocket>();

        ASSERT_TRUE(connection->setNonBlockingMode(true));
        AbstractStreamSocket* connectionPtr = connection.get();
        m_connections.push_back(std::move(connection));
        connectionPtr->connectAsync(
            m_mappedEndpoint,
            std::bind(&StreamSocketAcceptance::onConnectionComplete, this, connectionPtr, _1));
    }

    void onConnectionComplete(
        AbstractStreamSocket* connectionPtr,
        SystemError::ErrorCode errorCode)
    {
        ASSERT_TRUE(errorCode == SystemError::noError);

        std::unique_lock<std::mutex> lk(m_mutex);

        auto iterToRemove = std::remove_if(
            m_connections.begin(), m_connections.end(),
            [connectionPtr](const std::unique_ptr<AbstractStreamSocket>& elem) -> bool
            {
                return elem.get() == connectionPtr;
            });
        if (iterToRemove != m_connections.end())
        {
            ++m_completedConnectionsCount;
            m_connections.erase(iterToRemove, m_connections.end());
        }

        while (m_connections.size() < kConcurrentConnections)
        {
            if ((++m_startedConnectionsCount) <= kTotalConnections)
                startAnotherSocketNonSafe();
            else
                break;
        }
    }

    //---------------------------------------------------------------------------------------------
    // (end) Stopping multiple connections test.

private:
    SocketAddress m_mappedEndpoint;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectResultQueue;
    nx::utils::promise<int> m_serverPort;
    std::unique_ptr<typename SocketTypeSet::ServerSocket> m_serverSocket;
    std::unique_ptr<typename SocketTypeSet::ClientSocket> m_connection;

    void saveConnectResult(SystemError::ErrorCode connectResult)
    {
        m_connectResultQueue.push(connectResult);
    }

    void streamingServerMain(bool doServerDelay)
    {
        const auto server = std::make_unique<typename SocketTypeSet::ServerSocket>(
            SocketFactory::tcpClientIpVersion());

        ASSERT_TRUE(server->bind(SocketAddress::anyAddress));
        ASSERT_TRUE(server->listen());
        m_serverPort.set_value(server->getLocalAddress().port);

        std::unique_ptr<AbstractStreamSocket> client(server->accept());
        ASSERT_TRUE((bool)client);
        client->setRecvTimeout(kClientDelay);

        QByteArray wholeData;
        wholeData.reserve(kBufferSize * sizeof(int) * kIterations);
        std::vector<char> buffer(1024 * 1024);
        for (;;)
        {
            auto recv = client->recv(buffer.data(), (int)buffer.size());
            if (recv > 0)
            {
                wholeData.append(buffer.data(), recv);
            }
            else if (recv == 0)
            {
                break;
            }
            else
            {
                auto errCode = SystemError::getLastOSErrorCode();
                if (errCode != SystemError::timedOut && errCode != SystemError::again)
                {
                    ASSERT_EQ(0, errCode);
                    ASSERT_TRUE(client->isConnected());
                }
            }
            if (doServerDelay)
                std::this_thread::sleep_for(kClientDelay);
        }
        ASSERT_EQ((std::size_t)wholeData.size(), kBufferSize * sizeof(int) * kIterations);
        const int* testData = (const int*)wholeData.data();
        const int* endData = (const int*)(wholeData.data() + wholeData.size());
        int expectedValue = 0;
        while (testData < endData)
        {
            ASSERT_EQ(expectedValue, *testData);
            testData++;
            expectedValue = (expectedValue + 1) % kBufferSize;
        }
    }
};

TYPED_TEST_CASE_P(StreamSocketAcceptance);

// Windows 8/10 doesn't support client->server send timeout.
// It close connection automatically with error 10053 after client send timeout.
TYPED_TEST_P(StreamSocketAcceptance, DISABLED_receiveDelay)
{
    this->runStreamingTest(/*serverDelay*/ true, /*clientDelay*/ false);
}

TYPED_TEST_P(StreamSocketAcceptance, sendDelay)
{
    this->runStreamingTest(/*serverDelay */ false, /*clientDelay*/ true);
}

TYPED_TEST_P(StreamSocketAcceptance, uses_address_resolver)
{
    this->givenListeningServer();
    this->givenRandomNameMappedToServerHostIp();

    this->assertConnectionToServerCanBeEstablishedUsingMappedName();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    connect_including_resolve_is_cancelled_correctly)
{
    this->givenListeningServer();
    this->givenRandomNameMappedToServerHostIp();

    this->whenConnectUsingHostName();
    this->whenCancelAllSocketOperations();

    // then process does not crash.
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    connect_including_resolving_unknown_name_is_cancelled_correctly)
{
    this->givenRandomHostName();

    this->whenConnectUsingHostName();
    this->whenCancelAllSocketOperations();

    // then process does not crash.
}

TYPED_TEST_P(StreamSocketAcceptance, randomly_stopping_multiple_simultaneous_connections)
{
    this->givenListeningServer();
    this->givenRandomNameMappedToServerHostIp();

    this->startMaximumConcurrentConnections();

    int canCancelIndex = 0;
    int cancelledConnectionsCount = 0;
    std::unique_lock<std::mutex> lk(this->m_mutex);
    while ((this->m_completedConnectionsCount + cancelledConnectionsCount) < kTotalConnections)
    {
        std::unique_ptr<AbstractStreamSocket> connectionToCancel;
        if (this->m_connections.size() > 1 && (canCancelIndex < this->m_startedConnectionsCount))
        {
            auto connectionToCancelIter = this->m_connections.begin();
            size_t index = nx::utils::random::number<size_t>(0, this->m_connections.size() - 1);
            std::advance(connectionToCancelIter, index);
            connectionToCancel = std::move(*connectionToCancelIter);
            this->m_connections.erase(connectionToCancelIter);
            canCancelIndex += /*index +*/ 1;
        }
        lk.unlock();
        if (connectionToCancel)
        {
            connectionToCancel->pleaseStopSync();
            connectionToCancel.reset();
            ++cancelledConnectionsCount;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        lk.lock();
    }

    ASSERT_TRUE(this->m_connections.empty());
}

REGISTER_TYPED_TEST_CASE_P(StreamSocketAcceptance,
    DISABLED_receiveDelay,
    sendDelay,
    uses_address_resolver,
    connect_including_resolve_is_cancelled_correctly,
    connect_including_resolving_unknown_name_is_cancelled_correctly,
    randomly_stopping_multiple_simultaneous_connections);

} // namespace test
} // namespace network
} // namespace nx
