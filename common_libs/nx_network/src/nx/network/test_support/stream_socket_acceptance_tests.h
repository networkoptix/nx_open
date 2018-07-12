#pragma once

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/connection_server/simple_message_server.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/test_pipeline.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/string.h>

#include "synchronous_tcp_server.h"

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

/**
 * Adapted group of old socket tests.
 */
template<typename SocketTypeSet>
class SocketStreamingTestGroupFixture
{
protected:
    void runStreamingTest(bool doServerDelay, bool doClientDelay)
    {
        using namespace std::chrono;

        nx::utils::thread serverThread(
            std::bind(&SocketStreamingTestGroupFixture::streamingServerMain, this, doServerDelay));

        typename SocketTypeSet::ClientSocket clientSocket;
        const auto addr = m_serverAddress.get_future().get();
        ASSERT_TRUE(clientSocket.connect(addr, nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText().toStdString();
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
                        ASSERT_EQ(SystemError::noError, error);
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

private:
    nx::utils::promise<SocketAddress> m_serverAddress;

    void streamingServerMain(bool doServerDelay)
    {
        const auto server = std::make_unique<typename SocketTypeSet::ServerSocket>();

        ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(server->listen());
        m_serverAddress.set_value(server->getLocalAddress());

        auto client = server->accept();
        ASSERT_TRUE((bool)client);
        ASSERT_TRUE(client->setRecvTimeout(kClientDelay));

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
                if (errCode != SystemError::timedOut &&
                    errCode != SystemError::again &&
                    errCode != SystemError::wouldBlock)
                {
                    ASSERT_EQ(SystemError::noError, errCode);
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

//-------------------------------------------------------------------------------------------------

template<typename SocketTypeSet>
class StreamSocketStoppingMultipleConnectionsTestFixture
{
public:
    StreamSocketStoppingMultipleConnectionsTestFixture():
        m_startedConnectionsCount(0),
        m_completedConnectionsCount(0)
    {
    }

protected:
    void startMaximumConcurrentConnections(const SocketAddress& serverEndpoint)
    {
        m_serverEndpoint = serverEndpoint;
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
        typename SocketTypeSet::ClientSocket* connectionPtr = connection.get();
        m_connections.push_back(std::move(connection));
        connectionPtr->connectAsync(
            m_serverEndpoint,
            std::bind(&StreamSocketStoppingMultipleConnectionsTestFixture::onConnectionComplete,
                this, connectionPtr, _1));
    }

    void onConnectionComplete(
        typename SocketTypeSet::ClientSocket* connectionPtr,
        SystemError::ErrorCode errorCode)
    {
        ASSERT_TRUE(errorCode == SystemError::noError);

        std::unique_lock<std::mutex> lk(m_mutex);

        auto iterToRemove = std::remove_if(
            m_connections.begin(), m_connections.end(),
            [connectionPtr](const auto& elem) -> bool
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

    int startedConnectionsCount() const
    {
        return m_startedConnectionsCount;
    }

    int completedConnectionsCount() const
    {
        return m_completedConnectionsCount;
    }

    std::mutex m_mutex;
    std::deque<std::unique_ptr<typename SocketTypeSet::ClientSocket>> m_connections;

private:
    SocketAddress m_serverEndpoint;
    std::atomic<int> m_startedConnectionsCount;
    std::atomic<int> m_completedConnectionsCount;
};

//-------------------------------------------------------------------------------------------------

/**
 * Writes and reads socket concurrently using synchronous socket API.
 * Two threads are created internally for that purpose.
 */
template<typename SocketType>
class ConcurrentSocketPipe
{
public:
    ConcurrentSocketPipe(
        std::unique_ptr<SocketType> socket,
        nx::utils::SyncQueue<nx::Buffer>* dataToSendQueue,
        nx::utils::SyncQueue<nx::Buffer>* dataReceivedQueue)
        :
        m_socket(std::move(socket)),
        m_dataToSendQueue(dataToSendQueue),
        m_dataReceivedQueue(dataReceivedQueue),
        m_terminated(false)
    {
    }

    ~ConcurrentSocketPipe()
    {
        m_terminated = true;

        if (m_readThread.joinable())
            m_readThread.join();
        if (m_writeThread.joinable())
            m_writeThread.join();
    }

    bool start()
    {
        if (!m_socket->setRecvTimeout(std::chrono::milliseconds(1)))
            return false;

        m_readThread = std::thread(
            std::bind(&ConcurrentSocketPipe::readThreadMain, this));
        m_writeThread = std::thread(
            std::bind(&ConcurrentSocketPipe::writeThreadMain, this));

        return true;
    }

private:
    std::unique_ptr<SocketType> m_socket;
    nx::utils::SyncQueue<nx::Buffer>* m_dataToSendQueue;
    nx::utils::SyncQueue<nx::Buffer>* m_dataReceivedQueue;
    std::atomic<bool> m_terminated;
    std::thread m_readThread;
    std::thread m_writeThread;

    void readThreadMain()
    {
        while (!m_terminated)
        {
            nx::Buffer readBuf;
            readBuf.reserve(4*1024);
            int bytesRead = m_socket->recv(readBuf.data(), readBuf.capacity(), 0);
            if (bytesRead < 0)
            {
                if (SystemError::getLastOSErrorCode() == SystemError::timedOut ||
                    SystemError::getLastOSErrorCode() == SystemError::wouldBlock)
                {
                    continue;
                }

                break;
            }

            if (bytesRead == 0)
                break;

            readBuf.resize(bytesRead);
            m_dataReceivedQueue->push(std::move(readBuf));
        }
    }

    void writeThreadMain()
    {
        while (!m_terminated)
        {
            auto dataToSend = m_dataToSendQueue->pop(std::chrono::milliseconds(1));
            if (!dataToSend)
                continue;

            if (m_socket->send(dataToSend->data(), dataToSend->size()) < 0)
                break;
        }
    }
};

//-------------------------------------------------------------------------------------------------

template<typename SocketTypeSet>
class StreamSocketAcceptance:
    public ::testing::Test,
    public SocketStreamingTestGroupFixture<SocketTypeSet>,
    public StreamSocketStoppingMultipleConnectionsTestFixture<SocketTypeSet>
{
public:
    StreamSocketAcceptance():
        m_addressResolver(&nx::network::SocketGlobals::addressResolver()),
        m_clientMessage(nx::utils::random::generateName(17)),
        m_serverMessage(nx::utils::random::generateName(17))
    {
    }

    ~StreamSocketAcceptance()
    {
        if (m_connection)
        {
            m_connection->pleaseStopSync();
            m_connection.reset();
        }
        if (m_serverSocket)
            m_serverSocket->pleaseStopSync();
        if (m_server)
            m_server->pleaseStopSync();
        if (m_synchronousServer)
            m_synchronousServer->stop();

        for (const auto& connectionContext: m_clientConnections)
            connectionContext->connection.pleaseStopSync();
    }

protected:
    AddressResolver* m_addressResolver = nullptr;

    void givenListeningServerSocket(
        int backLogSize = AbstractStreamServerSocket::kDefaultBacklogSize)
    {
        m_serverSocket = std::make_unique<typename SocketTypeSet::ServerSocket>();
        ASSERT_TRUE(m_serverSocket->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_serverSocket->listen(backLogSize));
    }

    void givenAcceptingServerSocket()
    {
        givenListeningServerSocket();
        startAcceptingConnections();
    }

    void givenListeningNonBlockingServerSocket()
    {
        givenListeningServerSocket();
        ASSERT_TRUE(m_serverSocket->setNonBlockingMode(true));
    }

    void givenListeningSynchronousServer()
    {
        m_synchronousServer = std::make_unique<SynchronousReceivingServer>(
            std::make_unique<typename SocketTypeSet::ServerSocket>(),
            &m_synchronousServerReceivedData);
        ASSERT_TRUE(m_synchronousServer->bindAndListen(SocketAddress::anyPrivateAddress));
        m_synchronousServer->start();
    }

    void givenSilentServer()
    {
        givenListeningServerSocket();
    }

    void givenRandomNameMappedToServerHostIp()
    {
        givenRandomHostName();
        m_mappedEndpoint.port = serverEndpoint().port;

        m_addressResolver->addFixedAddress(
            m_mappedEndpoint.address,
            serverEndpoint());
    }

    void givenRandomHostName()
    {
        m_mappedEndpoint.address = nx::utils::random::generateName(7).toStdString();
        m_mappedEndpoint.port = nx::utils::random::number<std::uint16_t>();
    }

    void givenMessageServer()
    {
        m_server = std::make_unique<server::SimpleMessageServer>(
            std::make_unique<typename SocketTypeSet::ServerSocket>());
        m_server->setResponse(m_serverMessage);
        m_server->setKeepConnection(true);
        ASSERT_TRUE(m_server->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_server->listen());
    }

    void givenConnectedSocket()
    {
        m_connection = std::make_unique<typename SocketTypeSet::ClientSocket>();
        ASSERT_TRUE(m_connection->connect(
            serverEndpoint(), nx::network::kNoTimeout));
    }

    void givenPingPongServer()
    {
        givenMessageServer();
        m_server->setRequest(m_clientMessage);
    }

    void givenSocketInConnectStage()
    {
        givenListeningServerSocket();
        givenRandomNameMappedToServerHostIp();

        whenConnectUsingHostName();
    }

    void whenReceivedMessageFromServerAsync(
        nx::utils::MoveOnlyFunc<void()> auxiliaryHandler)
    {
        m_auxiliaryRecvHandler.swap(auxiliaryHandler);

        ASSERT_TRUE(m_connection->setNonBlockingMode(true));
        continueReceiving();

        thenServerMessageIsReceived();
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

    void whenSendRandomDataToServer()
    {
        m_sentData = nx::utils::generateRandomName(256);
        ASSERT_EQ(
            m_sentData.size(),
            m_connection->send(m_sentData.constData(), m_sentData.size()))
            << SystemError::getLastOSErrorText().toStdString();
    }

    void whenSendAsyncRandomDataToServer()
    {
        ASSERT_TRUE(m_connection->setNonBlockingMode(true));

        m_sentData = nx::utils::generateRandomName(16*1024);
        m_connection->sendAsync(
            m_sentData,
            [this](SystemError::ErrorCode /*systemErrorCode*/, std::size_t /*bytesSent*/)
            {
            });
    }

    void whenServerReadsWithFlag(int recvFlags)
    {
        auto acceptedConnection = m_serverSocket->accept();

        std::basic_string<uint8_t> readBuf(m_sentData.size(), 'x');
        ASSERT_EQ(
            m_sentData.size(),
            acceptedConnection->recv(readBuf.data(), (unsigned int) readBuf.size(), recvFlags))
            << SystemError::getLastOSErrorText().toStdString();

        m_synchronousServerReceivedData.write(readBuf.data(), m_sentData.size());
    }

    void startReadingConnectionAsync()
    {
        ASSERT_TRUE(m_connection->setNonBlockingMode(true));

        m_waitForSingleRecvResult = false;
        continueReceiving();
    }

    void continueReceiving()
    {
        using namespace std::placeholders;

        m_readBuffer.reserve(m_readBuffer.size() + 1024);
        m_connection->readSomeAsync(
            &m_readBuffer,
            std::bind(&StreamSocketAcceptance::saveReadResult, this, _1, _2));
    }

    void waitForConnectionRecvTimeout()
    {
        for (;;)
        {
            if (std::get<0>(m_recvResultQueue.pop()) == SystemError::timedOut)
                break;
        }
    }

    void whenCancelAllSocketOperations()
    {
        m_connection->pleaseStopSync();
    }

    void whenClientSentPing()
    {
        whenClientSendsPing();
        thenSendSucceeded();
    }

    void whenClientSendsPing()
    {
        const auto bytesSent =
            m_connection->send(m_clientMessage.constData(), m_clientMessage.size());

        m_sendResultQueue.push(
            bytesSent == m_clientMessage.size()
            ? SystemError::noError
            : SystemError::getLastOSErrorCode());
    }

    void whenClientSentPingAsync()
    {
        whenClientSendsPingAsync();
        thenSendSucceeded();
    }

    void whenClientSendsPingAsync()
    {
        m_connection->sendAsync(
            m_clientMessage,
            [this](SystemError::ErrorCode systemErrorCode, std::size_t /*bytesSent*/)
            {
                m_sendResultQueue.push(systemErrorCode);
            });
    }

    void thenSendSucceeded()
    {
        ASSERT_EQ(SystemError::noError, m_sendResultQueue.pop());
    }

    void thenSendFailedWith(SystemError::ErrorCode systemErrorCode)
    {
        ASSERT_EQ(systemErrorCode, m_sendResultQueue.pop());
    }

    void thenSendFailedUnrecoverableError()
    {
        const auto errorCode = m_sendResultQueue.pop();
        ASSERT_NE(SystemError::noError, errorCode);
        ASSERT_TRUE(socketCannotRecoverFromError(errorCode));
    }

    void whenClientSendsRandomDataAsyncNonStop()
    {
        ASSERT_TRUE(m_connection->setNonBlockingMode(true));
        if (m_randomDataBuffer.isEmpty())
            m_randomDataBuffer = nx::utils::generateRandomName(64*1024);

        m_connection->sendAsync(
            m_randomDataBuffer,
            [this](SystemError::ErrorCode systemErrorCode, std::size_t /*bytesSent*/)
            {
                if (systemErrorCode == SystemError::noError)
                {
                    whenClientSendsRandomDataAsyncNonStop();
                    return;
                }

                m_sendResultQueue.push(systemErrorCode);
            });
    }

    void whenSendMultiplePingsViaMultipleConnections(int connectionCount)
    {
        m_expectedResponseCount = connectionCount;
        for (int i = 0; i < m_expectedResponseCount; ++i)
        {
            m_clientConnections.push_back(
                std::make_unique<ClientConnectionContext>());
            ASSERT_TRUE(m_clientConnections.back()->connection.connect(
                serverEndpoint(), kNoTimeout));
            ASSERT_TRUE(m_clientConnections.back()->connection.setNonBlockingMode(true));
            m_clientConnections.back()->buffer.reserve(m_serverMessage.size());

            m_clientConnections.back()->connection.sendAsync(
                m_clientMessage,
                [this, connectionCtx = m_clientConnections.back().get()](
                    SystemError::ErrorCode errorCode,
                    std::size_t bytesTransferred)
                {
                    if (errorCode != SystemError::noError ||
                        (int)bytesTransferred != m_clientMessage.size())
                    {
                        // m_recvResultQueue serves as result storage in this particular test.
                        m_recvResultQueue.push(std::make_tuple(errorCode, nx::Buffer()));
                        return;
                    }

                    connectionCtx->connection.readAsyncAtLeast(
                        &connectionCtx->buffer, m_serverMessage.size(),
                        [this, connectionCtx](
                            SystemError::ErrorCode errorCode, size_t /*size*/)
                        {
                            m_recvResultQueue.push(
                                std::make_tuple(errorCode, connectionCtx->buffer));
                        });
                });
        }
    }

    void whenReadSocketInBlockingWay()
    {
        whenReadSocketInBlockingWayWithFlags(0);
    }

    void whenReadSocketInBlockingWayWithFlags(int flags)
    {
        nx::Buffer readBuf;
        readBuf.resize(64 * 1024);
        int bytesRead = m_connection->recv(readBuf.data(), readBuf.capacity(), flags);
        if (bytesRead >= 0)
        {
            readBuf.resize(bytesRead);
            m_recvResultQueue.push({ SystemError::noError, readBuf });
        }
        else
        {
            m_recvResultQueue.push({ SystemError::getLastOSErrorCode(), readBuf });
        }
    }

    void whenAcceptNonBlocking()
    {
        ASSERT_TRUE(m_serverSocket->setNonBlockingMode(true))
            << SystemError::getLastOSErrorText().toStdString();

        whenAcceptConnection();
    }

    void whenAcceptConnection()
    {
        auto acceptedConnection = m_serverSocket->accept();
        const auto systemErrorCode =
            acceptedConnection == nullptr
            ? SystemError::getLastOSErrorCode()
            : SystemError::noError;
        m_acceptedConnections.push(
            std::make_tuple(systemErrorCode, std::move(acceptedConnection)));
    }

    void waitUntilConnectionIsAcceptedInNonBlockingMode()
    {
        ASSERT_TRUE(m_serverSocket->setNonBlockingMode(true))
            << SystemError::getLastOSErrorText().toStdString();

        waitUntilConnectionIsAccepted();
    }

    void waitUntilConnectionIsAccepted()
    {
        for (;;)
        {
            whenAcceptConnection();

            m_prevAcceptResult = m_acceptedConnections.pop();
            if (std::get<0>(m_prevAcceptResult) == SystemError::wouldBlock)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            ASSERT_NE(nullptr, std::move(std::get<1>(m_prevAcceptResult)));
            break;
        }
    }

    void whenConnectionIsAccepted()
    {
        this->givenConnectedSocket();
        // E.g., for socket with encryption auto-detection
        this->whenClientSentPing();

        this->waitUntilConnectionIsAccepted();
    }

    void whenEstablishMultipleConnectionsAsync(int connectionCount)
    {
        m_clientConnections.resize(connectionCount);

        for (auto& connectionCtx: m_clientConnections)
        {
            connectionCtx = std::make_unique<ClientConnectionContext>();
            ASSERT_TRUE(connectionCtx->connection.setNonBlockingMode(true));
            connectionCtx->connection.connectAsync(
                serverEndpoint(),
                [this, &connectionCtx](SystemError::ErrorCode systemErrorCode)
                {
                    m_connectResultQueue.push(systemErrorCode);
                    if (systemErrorCode != SystemError::noError)
                        return;

                    // TODO: #ak Have to send something here because server socket with
                    // encryption auto-detection will not return socket until something is received.
                    // This condition should be handled by a more generic approach.
                    connectionCtx->connection.sendAsync(
                        m_clientMessage,
                        [](SystemError::ErrorCode, std::size_t) {});
                });
        }
    }

    void assertAcceptedConnectionNonBlockingModeIs(bool expected)
    {
        bool isNonBlockingMode = false;
        ASSERT_TRUE(std::get<1>(m_prevAcceptResult)->getNonBlockingMode(&isNonBlockingMode));
        ASSERT_EQ(expected, isNonBlockingMode);
    }

    void thenAcceptReported(SystemError::ErrorCode expected)
    {
        m_prevAcceptResult = m_acceptedConnections.pop();

        ASSERT_EQ(expected, std::get<0>(m_prevAcceptResult));
    }

    void thenConnectionHasBeenAccepted()
    {
        thenAcceptReported(SystemError::noError);

        ASSERT_NE(nullptr, std::get<1>(m_prevAcceptResult));
    }

    void thenConnectionIsEstablished()
    {
        ASSERT_EQ(SystemError::noError, m_connectResultQueue.pop());
    }

    void thenEveryConnectionEstablishedSuccessfully()
    {
        for (int i = 0; i < (int) m_clientConnections.size(); ++i)
        {
            ASSERT_EQ(SystemError::noError, m_connectResultQueue.pop());
        }
    }

    void assertConnectionToServerCanBeEstablishedUsingMappedName()
    {
        whenConnectUsingHostName();
        thenConnectionIsEstablished();
    }

    void setClientSocketRecvTimeout(std::chrono::milliseconds timeout)
    {
        ASSERT_TRUE(m_connection->setRecvTimeout(timeout.count()));
    }

    void setClientSocketSendTimeout(std::chrono::milliseconds timeout)
    {
        ASSERT_TRUE(m_connection->setSendTimeout(timeout.count()));
    }

    void thenServerMessageIsReceived()
    {
        for (;;)
        {
            const auto prevRecvResult = m_recvResultQueue.pop();
            if (std::get<0>(prevRecvResult) != SystemError::noError)
                continue;

            ASSERT_EQ(SystemError::noError, std::get<0>(prevRecvResult));
            ASSERT_EQ(m_serverMessage, std::get<1>(prevRecvResult));
            break;
        }
    }

    void thenClientSocketReported(SystemError::ErrorCode expected)
    {
        const auto prevRecvResult = m_recvResultQueue.pop();
        ASSERT_EQ(expected, std::get<0>(prevRecvResult));
    }

    void thenClientSocketReportedTimedout()
    {
        thenClientSocketReported(SystemError::timedOut);
    }

    void thenClientSendTimesOutEventually()
    {
        ASSERT_EQ(SystemError::timedOut, m_sendResultQueue.pop());
    }

    void thenClientSocketReportedFailure()
    {
        const auto prevRecvResult = m_recvResultQueue.pop();
        ASSERT_NE(SystemError::noError, std::get<0>(prevRecvResult));
    }

    void thenPongIsReceivedViaEachConnection()
    {
        for (int i = 0; i < m_expectedResponseCount; ++i)
        {
            auto recvResult = m_recvResultQueue.pop();
            ASSERT_EQ(SystemError::noError, std::get<0>(recvResult));
            ASSERT_EQ(m_serverMessage, std::get<1>(recvResult));
        }
    }

    void thenSocketCanBeSafelyRemoved()
    {
        m_connection.reset();
    }

    void thenServerReceivedData()
    {
        m_synchronousServerReceivedData.waitForReceivedDataToMatch(m_sentData);
    }

    void thenEveryConnectionIsAccepted()
    {
        for (int i = 0; i < (int) m_clientConnections.size(); ++i)
        {
            ASSERT_NE(nullptr, std::get<1>(m_acceptedConnections.pop()));
        }
    }

    //---------------------------------------------------------------------------------------------

    void whenSendDataConcurrentlyThroughConnectedSockets()
    {
        m_sentData = nx::utils::generateRandomName(16 * 1024);

        givenListeningServerSocket();
        startAcceptingConnections();

        givenConnectedSocket();
        m_concurrentSocketPipes.push_back(
            std::make_unique<ConcurrentSocketPipeContext>(
                std::exchange(m_connection, nullptr)));
        m_concurrentSocketPipes.back()->dataToSendQueue.push(m_sentData);

        auto acceptResult = m_acceptedConnections.pop();
        ASSERT_NE(nullptr, std::get<1>(acceptResult));
        m_concurrentSocketPipes.push_back(
            std::make_unique<ConcurrentSocketPipeContext>(
                std::exchange(std::get<1>(acceptResult), nullptr)));
        m_concurrentSocketPipes.back()->dataToSendQueue.push(m_sentData);
    }

    void thenBothSocketsReceiveExpectedData()
    {
        for (auto& pipe: m_concurrentSocketPipes)
            pipe->waitUntilReceivedDataMatch(m_sentData);
    }

    //---------------------------------------------------------------------------------------------

    SocketAddress mappedEndpoint() const
    {
        return m_mappedEndpoint;
    }

    SocketAddress serverEndpoint() const
    {
        if (m_server)
            return SocketAddress(m_server->address());
        else if (m_serverSocket)
            return m_serverSocket->getLocalAddress();
        else if (m_synchronousServer)
            return m_synchronousServer->endpoint();
        else
            return SocketAddress();
    }

    typename SocketTypeSet::ClientSocket* connection()
    {
        return m_connection.get();
    }

    typename SocketTypeSet::ServerSocket* serverSocket()
    {
        return m_serverSocket.get();
    }

    AbstractStreamSocket* lastAcceptedSocket()
    {
        return std::get<1>(m_prevAcceptResult).get();
    }

private:
    using RecvResult = std::tuple<SystemError::ErrorCode, nx::Buffer>;
    using AcceptResult =
        std::tuple<SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>>;

    struct ClientConnectionContext
    {
        typename SocketTypeSet::ClientSocket connection;
        nx::Buffer buffer;
    };

    int m_expectedResponseCount = 0;
    SocketAddress m_mappedEndpoint;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectResultQueue;
    const nx::Buffer m_clientMessage;
    const nx::Buffer m_serverMessage;
    nx::Buffer m_readBuffer;
    nx::utils::SyncQueue<RecvResult> m_recvResultQueue;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_sendResultQueue;
    nx::Buffer m_randomDataBuffer;
    std::unique_ptr<typename SocketTypeSet::ServerSocket> m_serverSocket;
    std::unique_ptr<typename SocketTypeSet::ClientSocket> m_connection;
    std::unique_ptr<server::SimpleMessageServer> m_server;
    nx::utils::MoveOnlyFunc<void()> m_auxiliaryRecvHandler;
    std::vector<std::unique_ptr<ClientConnectionContext>> m_clientConnections;
    nx::utils::SyncQueue<AcceptResult> m_acceptedConnections;
    AcceptResult m_prevAcceptResult;
    bool m_waitForSingleRecvResult = true;

    //---------------------------------------------------------------------------------------------
    // Concurrent I/O.

    class ConcurrentSocketPipeContext
    {
    public:
        nx::utils::SyncQueue<nx::Buffer> dataToSendQueue;
        nx::utils::SyncQueue<nx::Buffer> dataReceivedQueue;
        ConcurrentSocketPipe<AbstractStreamSocket> pipe;

        ConcurrentSocketPipeContext(std::unique_ptr<AbstractStreamSocket> socket):
            pipe(
                std::move(socket),
                &dataToSendQueue,
                &dataReceivedQueue)
        {
            pipe.start();
        }

        void waitUntilReceivedDataMatch(const nx::Buffer& expected)
        {
            nx::Buffer received;
            while (!received.startsWith(expected))
                received += dataReceivedQueue.pop();
        }
    };

    std::vector<std::unique_ptr<ConcurrentSocketPipeContext>> m_concurrentSocketPipes;

    //---------------------------------------------------------------------------------------------

    std::unique_ptr<SynchronousReceivingServer> m_synchronousServer;
    nx::Buffer m_sentData;
    nx::utils::bstream::test::NotifyingOutput m_synchronousServerReceivedData;

    void saveConnectResult(SystemError::ErrorCode connectResult)
    {
        m_connectResultQueue.push(connectResult);
    }

    void saveReadResult(SystemError::ErrorCode systemErrorCode, std::size_t bytesRead)
    {
        if (systemErrorCode == SystemError::noError && bytesRead > 0)
        {
            if (m_readBuffer != m_serverMessage)
            {
                continueReceiving();
                return;
            }

            m_recvResultQueue.push(
                std::make_tuple(SystemError::noError, m_readBuffer));
            m_readBuffer.clear();
        }
        else
        {
            m_recvResultQueue.push(std::make_tuple(systemErrorCode, nx::Buffer()));
        }

        if (m_auxiliaryRecvHandler)
            nx::utils::swapAndCall(m_auxiliaryRecvHandler);

        if (!m_waitForSingleRecvResult)
            continueReceiving();
    }

    void startAcceptingConnections()
    {
        ASSERT_TRUE(m_serverSocket->setNonBlockingMode(true));

        m_serverSocket->acceptAsync(
            [this](
                SystemError::ErrorCode systemErrorCode,
                std::unique_ptr<AbstractStreamSocket> connection)
            {
                if (connection)
                {
                    m_acceptedConnections.push(
                        std::make_tuple(
                            systemErrorCode,
                            std::move(connection)));
                }

                this->startAcceptingConnections();
            });
    }
};

TYPED_TEST_CASE_P(StreamSocketAcceptance);

//-------------------------------------------------------------------------------------------------

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
    this->givenMessageServer();
    this->givenRandomNameMappedToServerHostIp();

    this->assertConnectionToServerCanBeEstablishedUsingMappedName();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    connect_including_resolve_is_cancelled_correctly)
{
    this->givenListeningServerSocket();
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
    this->givenListeningServerSocket(kTotalConnections);
    this->givenRandomNameMappedToServerHostIp();

    this->startMaximumConcurrentConnections(this->mappedEndpoint());

    // TODO: #ak Refactor this test

    int canCancelIndex = 0;
    int cancelledConnectionsCount = 0;
    std::unique_lock<std::mutex> lk(this->m_mutex);
    while ((this->completedConnectionsCount() + cancelledConnectionsCount) < kTotalConnections)
    {
        std::unique_ptr<AbstractStreamSocket> connectionToCancel;
        if (this->m_connections.size() > 1 && (canCancelIndex < this->startedConnectionsCount()))
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

TYPED_TEST_P(StreamSocketAcceptance, receive_timeout_change_is_not_ignored)
{
    this->givenPingPongServer();
    this->givenConnectedSocket();

    this->whenClientSentPing();
    this->whenReceivedMessageFromServerAsync(
        [this]()
        {
            this->setClientSocketRecvTimeout(std::chrono::milliseconds(1));
            this->continueReceiving();
        });

    this->thenClientSocketReportedTimedout();
    //this->thenClientSocketReportedFailure();
}

// TODO: #ak Modify and uncomment this test.
// But, the use case is not valid in case of encryption
// auto-detection is enabled on server side.
//TYPED_TEST_P(StreamSocketAcceptance, server_sends_first)
//{
//    this->givenMessageServer();
//    this->givenConnectedSocket();
//
//    this->whenReadClientSocket();

//    this->thenServerMessageIsReceived();
//}

TYPED_TEST_P(StreamSocketAcceptance, transfer_async)
{
    constexpr int connectionCount = 7;

    this->givenPingPongServer();
    this->whenSendMultiplePingsViaMultipleConnections(connectionCount);
    this->thenPongIsReceivedViaEachConnection();
}

TYPED_TEST_P(StreamSocketAcceptance, transfer_sync)
{
    this->givenListeningSynchronousServer();
    this->givenConnectedSocket();

    this->whenSendRandomDataToServer();

    this->thenServerReceivedData();
}

TYPED_TEST_P(StreamSocketAcceptance, recv_sync_with_wait_all_flag)
{
    this->givenListeningServerSocket();
    this->givenConnectedSocket();

    this->whenSendAsyncRandomDataToServer();
    this->whenServerReadsWithFlag(MSG_WAITALL);

    this->thenServerReceivedData();
}

TYPED_TEST_P(StreamSocketAcceptance, recv_timeout_is_reported)
{
    this->givenSilentServer();
    this->givenConnectedSocket();
    this->setClientSocketRecvTimeout(std::chrono::milliseconds(1));

    this->whenReadSocketInBlockingWay();

    this->thenClientSocketReportedTimedout();
    //this->thenClientSocketReportedFailure();
}

TYPED_TEST_P(StreamSocketAcceptance, msg_dont_wait_flag_makes_recv_call_nonblocking)
{
    this->givenSilentServer();
    this->givenConnectedSocket();

    this->whenReadSocketInBlockingWayWithFlags(MSG_DONTWAIT);

    this->thenClientSocketReported(SystemError::wouldBlock);
}

TYPED_TEST_P(StreamSocketAcceptance, async_connect_is_cancelled_by_cancelling_write)
{
    this->givenSocketInConnectStage();

    this->connection()->cancelIOSync(aio::etWrite);

    this->thenSocketCanBeSafelyRemoved();
}

TYPED_TEST_P(StreamSocketAcceptance, concurrent_recv_send_in_blocking_mode)
{
    this->whenSendDataConcurrentlyThroughConnectedSockets();
    this->thenBothSocketsReceiveExpectedData();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    nonblocking_accept_reports_wouldBlock_if_no_incoming_connections)
{
    this->givenListeningServerSocket();
    this->whenAcceptNonBlocking();
    this->thenAcceptReported(SystemError::wouldBlock);
}

TYPED_TEST_P(StreamSocketAcceptance, nonblocking_accept_actually_accepts_connections)
{
    this->givenListeningServerSocket();
    this->givenConnectedSocket();
    // E.g., for socket with encryption auto-detection
    this->whenClientSentPing();

    this->waitUntilConnectionIsAcceptedInNonBlockingMode();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    accepted_socket_is_in_blocking_mode_when_server_socket_is_nonblocking)
{
    this->givenListeningNonBlockingServerSocket();
    this->whenConnectionIsAccepted();
    this->assertAcceptedConnectionNonBlockingModeIs(false);
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    accepted_socket_is_in_blocking_mode_when_server_socket_is_blocking)
{
    this->givenListeningServerSocket();
    this->whenConnectionIsAccepted();
    this->assertAcceptedConnectionNonBlockingModeIs(false);
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    server_socket_accepts_many_connections_in_a_row)
{
    constexpr int connectionCount = 51;

    this->givenAcceptingServerSocket();
    this->whenEstablishMultipleConnectionsAsync(connectionCount);
    this->thenEveryConnectionIsAccepted();
}

// TODO: #ak Following test is not relevant for Macosx since server socket there
// has a queue of connect requests, not connections with fulfilled handshake.
// Adapt for Mac or erase.
TYPED_TEST_P(StreamSocketAcceptance, DISABLED_server_socket_listen_queue_size_is_used)
{
    constexpr int listenQueueSize =
        AbstractStreamServerSocket::kDefaultBacklogSize + 11;

    this->givenListeningServerSocket(listenQueueSize + 10);
    this->whenEstablishMultipleConnectionsAsync(listenQueueSize);
    this->thenEveryConnectionEstablishedSuccessfully();
}

TYPED_TEST_P(StreamSocketAcceptance, socket_is_reusable_after_recv_timeout)
{
    this->givenPingPongServer();

    this->givenConnectedSocket();
    this->setClientSocketRecvTimeout(std::chrono::milliseconds(1));

    this->startReadingConnectionAsync();
    this->waitForConnectionRecvTimeout();

    this->whenClientSentPingAsync();
    this->thenServerMessageIsReceived();
}

TYPED_TEST_P(StreamSocketAcceptance, socket_reports_send_timeout)
{
    this->givenAcceptingServerSocket();
    this->givenConnectedSocket();
    this->setClientSocketSendTimeout(std::chrono::milliseconds(1));

    this->whenClientSendsRandomDataAsyncNonStop();

    this->thenClientSendTimesOutEventually();
}

TYPED_TEST_P(StreamSocketAcceptance, cancel_io)
{
    this->givenPingPongServer();
    this->givenConnectedSocket();

    this->whenClientSentPing();
    this->whenReceivedMessageFromServerAsync(
        [this]()
        {
            this->connection()->cancelIOSync(aio::etNone);
        });
}

REGISTER_TYPED_TEST_CASE_P(StreamSocketAcceptance,
    DISABLED_receiveDelay,
    sendDelay,
    uses_address_resolver,
    connect_including_resolve_is_cancelled_correctly,
    connect_including_resolving_unknown_name_is_cancelled_correctly,
    randomly_stopping_multiple_simultaneous_connections,
    receive_timeout_change_is_not_ignored,
    transfer_async,
    transfer_sync,
    recv_sync_with_wait_all_flag,
    recv_timeout_is_reported,
    msg_dont_wait_flag_makes_recv_call_nonblocking,
    async_connect_is_cancelled_by_cancelling_write,
    concurrent_recv_send_in_blocking_mode,
    nonblocking_accept_reports_wouldBlock_if_no_incoming_connections,
    nonblocking_accept_actually_accepts_connections,
    accepted_socket_is_in_blocking_mode_when_server_socket_is_nonblocking,
    accepted_socket_is_in_blocking_mode_when_server_socket_is_blocking,
    server_socket_accepts_many_connections_in_a_row,
    DISABLED_server_socket_listen_queue_size_is_used,
    socket_is_reusable_after_recv_timeout,
    socket_reports_send_timeout,
    cancel_io);

} // namespace test
} // namespace network
} // namespace nx
