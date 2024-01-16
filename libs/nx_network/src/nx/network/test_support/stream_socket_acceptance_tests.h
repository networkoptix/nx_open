// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Contains template tests that verify the behavior of an nx::network::AbstractStreamSocket
 * implementation.
 * Refer to an existing StreamSocketAcceptance usage for an example.
 */

#pragma once

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/connection_server/simple_message_server.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/test_pipeline.h>
#include <nx/utils/thread/sync_queue.h>

#include "synchronous_tcp_server.h"

namespace nx {
namespace network {
namespace test {

namespace {

constexpr int kIterations = 100;
constexpr int kBufferSize = 1024 * 32;
constexpr std::chrono::milliseconds kClientDelay(1);
constexpr int kConcurrentConnections = 11;
constexpr int kTotalConnections = 37;

} // namespace

/**
 * Adapted group of old socket tests.
 */
template<typename SocketTypeSet>
class SocketStreamingTestGroupFixture
{
protected:
    struct StreamingTestConfig
    {
        bool doServerDelay = false;
        bool doClientDelay = false;
        bool assertOnError = true;
    };

    bool runStreamingTest(const StreamingTestConfig& conf)
    {
        using namespace std::chrono;

        std::promise<bool> streamingServerResult;
        std::promise<SocketAddress> serverAddressAvailable;
        nx::utils::thread serverThread(
            [this, &conf, &serverAddressAvailable, &streamingServerResult]()
            {
                streamingServerResult.set_value(streamingServerMain(conf, &serverAddressAvailable));
            });

        typename SocketTypeSet::ClientSocket clientSocket;
        const auto addr = serverAddressAvailable.get_future().get();
        EXPECT_TRUE(clientSocket.connect(addr, nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText();
        if (conf.doServerDelay)
        {
            EXPECT_TRUE(clientSocket.setSendTimeout(duration_cast<milliseconds>(kClientDelay).count()));
        }

        std::vector<int> buffer(kBufferSize);
        for (int i = 0; i < kBufferSize; ++i)
            buffer[i] = i;

        bool failed = false;
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
                        if (conf.assertOnError)
                        {
                            EXPECT_EQ(SystemError::noError, error);
                            failed = true;
                        }
                        break; //< Send error.
                    }
                }
            }
            if (conf.doClientDelay)
                std::this_thread::sleep_for(kClientDelay);
        }
        clientSocket.close();
        serverThread.join();

        return !failed && streamingServerResult.get_future().get();
    }

private:
    bool streamingServerMain(
        const StreamingTestConfig& conf,
        std::promise<SocketAddress>* serverAddressAvailable)
    {
        const auto server = std::make_unique<typename SocketTypeSet::ServerSocket>();

        EXPECT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
        EXPECT_TRUE(server->listen());
        serverAddressAvailable->set_value(server->getLocalAddress());

        auto client = server->accept();
        EXPECT_TRUE((bool)client);
        EXPECT_TRUE(client->setRecvTimeout(kClientDelay));

        constexpr int wholeDataSize = kBufferSize * sizeof(int) * kIterations;
        nx::Buffer wholeData;
        wholeData.reserve(wholeDataSize);
        std::vector<char> buffer(1024 * 1024);
        SystemError::ErrorCode error = SystemError::noError;
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
                error = SystemError::getLastOSErrorCode();
                if (error != SystemError::timedOut &&
                    error != SystemError::again &&
                    error != SystemError::interrupted &&
                    error != SystemError::wouldBlock)
                {
                    // Connection has been broken.
                    break;
                }
            }
            if (conf.doServerDelay)
                std::this_thread::sleep_for(kClientDelay);
        }

        if (conf.assertOnError)
        {
            EXPECT_EQ(wholeData.size(), wholeDataSize) << SystemError::toString(error);
        }

        if (wholeData.size() != wholeDataSize)
            return false;

        const int* testData = (const int*)wholeData.data();
        const int* endData = (const int*)(wholeData.data() + wholeData.size());
        int expectedValue = 0;
        while (testData < endData)
        {
            if (conf.assertOnError)
            {
                EXPECT_EQ(expectedValue, *testData);
            }

            if (expectedValue != *testData)
                return false;

            testData++;
            expectedValue = (expectedValue + 1) % kBufferSize;
        }

        return true;
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
            NX_MUTEX_LOCKER lk(&m_mutex);
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
        ASSERT_EQ(SystemError::noError, errorCode);

        NX_MUTEX_LOCKER lk(&m_mutex);

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

    mutable nx::Mutex m_mutex;
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
        if (!m_socket->setNonBlockingMode(false) ||
            !m_socket->setRecvTimeout(std::chrono::milliseconds(1)))
        {
            return false;
        }

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
            readBuf.resize(4*1024);
            int bytesRead = m_socket->recv(readBuf.data(), readBuf.size(), 0);
            if (bytesRead < 0)
            {
                if (socketCannotRecoverFromError(SystemError::getLastOSErrorCode()))
                    break;
                continue;
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

            const auto size = dataToSend->size();
            auto remain = size;
            while (remain > 0)
            {
                const auto sent = m_socket->send(dataToSend->data() + (size - remain), remain);
                if (sent < 0)
                {
                    if (socketCannotRecoverFromError(SystemError::getLastOSErrorCode()))
                        return;
                    continue;
                }
                if (sent == 0)
                    return;
                remain -= sent;
            }
        }
    }
};

//-------------------------------------------------------------------------------------------------

enum class SocketTypeLimitation
{
    noConcurrentBlockingIo,
    notUsableAfterSendInterrupt,
};

template<typename SocketTypeSet>
class StreamSocketAcceptance:
    public ::testing::Test,
    public SocketStreamingTestGroupFixture<SocketTypeSet>,
    public StreamSocketStoppingMultipleConnectionsTestFixture<SocketTypeSet>
{
public:
    StreamSocketAcceptance():
        m_addressResolver(&nx::network::SocketGlobals::addressResolver()),
        m_clientMessage("request_" + nx::utils::random::generateName(17)),
        m_serverMessage("response_" + nx::utils::random::generateName(17))
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

    void setServerSocketAcceptTimeout(std::chrono::milliseconds timeout)
    {
        ASSERT_TRUE(m_serverSocket->setRecvTimeout(timeout.count()));
    }

    void givenAcceptingServerSocket()
    {
        givenListeningServerSocket();
        whenStartAcceptingConnections();
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

    void givenSynchronousPingPongServer()
    {
        m_synchronousServer = std::make_unique<SynchronousPingPongServer>(
            std::make_unique<typename SocketTypeSet::ServerSocket>(),
            m_clientMessage,
            m_serverMessage);
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
        m_mappedEndpoint.address = nx::utils::random::generateName(7);
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

    void givenClientSocket()
    {
        if (m_connection)
            m_connection->pleaseStopSync();

        m_connection = std::make_unique<typename SocketTypeSet::ClientSocket>();
    }

    void givenConnectedSocket()
    {
        givenClientSocket();
        whenSynchronouslyConnectToServer();
    }

    void givenPingPongServer()
    {
        givenMessageServer();
        m_server->setRequest(m_clientMessage);
    }

    void givenSpamServer()
    {
        m_synchronousServer = std::make_unique<SynchronousSpamServer>(
            std::make_unique<typename SocketTypeSet::ServerSocket>());
        ASSERT_TRUE(m_synchronousServer->bindAndListen(SocketAddress::anyPrivateAddress));
        m_synchronousServer->start();
    }

    void givenSocketInConnectStage()
    {
        givenListeningServerSocket();
        givenRandomNameMappedToServerHostIp();

        whenConnectUsingHostName();
    }

    void givenClientConnectionTimedOutOnSend()
    {
        givenAcceptingServerSocket();
        givenConnectedSocket();
        setClientSocketSendTimeout(std::chrono::milliseconds(1));

        whenClientSendsRandomDataSynchronouslyUntilFailure();
        thenClientSendTimesOutEventually();
    }

    void whenSynchronouslyConnectToServer()
    {
        ASSERT_TRUE(m_connection->connect(serverEndpoint(), nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText();
    }

    template<typename AuxiliaryConnectCompletionHandler>
    void whenConnectToServer(AuxiliaryConnectCompletionHandler&& handler)
    {
        givenClientSocket();
        whenConnectToServerAsync(serverEndpoint(), std::move(handler));
        thenConnectionIsEstablished();
    }

    template<typename AuxiliaryConnectCompletionHandler>
    void whenConnectToServerAsync(
        const SocketAddress& endpoint,
        AuxiliaryConnectCompletionHandler&& handler)
    {
        ASSERT_TRUE(m_connection->setNonBlockingMode(true));
        m_connection->connectAsync(
            endpoint,
            [this, handler = std::move(handler)](SystemError::ErrorCode resultCode)
            {
                handler();
                this->saveConnectResult(resultCode);
            });
    }

    void whenConnectToServerAsync()
    {
        whenConnectToServerAsync(serverEndpoint(), []() {});
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
        givenClientSocket();
        whenConnectToServerAsync(m_mappedEndpoint, [](){});
    }

    nx::Buffer whenSendRandomDataToServer()
    {
        m_sentData = nx::utils::generateRandomName(1023);
        EXPECT_EQ(
            m_sentData.size(),
            m_connection->send(m_sentData.data(), m_sentData.size()))
            << SystemError::getLastOSErrorText();

        return m_sentData;
    }

    void whenSendAsyncRandomDataToServer(
        std::function<void()> auxiliaryHandler = nullptr)
    {
        ASSERT_TRUE(m_connection->setNonBlockingMode(true));

        m_sentData = nx::utils::generateRandomName(16*1024);
        m_connection->sendAsync(
            &m_sentData,
            [auxiliaryHandler = std::move(auxiliaryHandler)](
                SystemError::ErrorCode, std::size_t)
            {
                if (auxiliaryHandler)
                    auxiliaryHandler();
            });
    }

    void whenServerReadsWithFlags(int recvFlags)
    {
        whenAcceptConnection();
        thenConnectionHasBeenAccepted();

        whenServerReadsBytesWithFlags(m_sentData.size(), recvFlags);
    }

    void whenServerReadsBytesWithFlags(std::size_t bytesExpected, int recvFlags)
    {
        std::basic_string<uint8_t> readBuf(bytesExpected, 'x');

        std::size_t totalBytesReceived = 0;
        while (totalBytesReceived < bytesExpected)
        {
            auto bytesReceived = std::get<1>(m_prevAcceptResult)->recv(
                readBuf.data(),
                (unsigned int) (bytesExpected - totalBytesReceived),
                recvFlags);
            ASSERT_GT(bytesReceived, 0) << SystemError::getLastOSErrorText();
            totalBytesReceived += bytesReceived;
            m_synchronousServerReceivedData.write(readBuf.data(), bytesReceived);
        }
    }

    std::tuple<int /*bytesReadCnt*/, std::basic_string<uint8_t> /*buf*/> whenServerReadsSome()
    {
        std::basic_string<uint8_t> readBuf(4096, 'x');

        int bytesReceived = std::get<1>(m_prevAcceptResult)->recv(readBuf.data(), readBuf.size(), 0);
        if (bytesReceived > 0)
        {
            m_synchronousServerReceivedData.write(readBuf.data(), bytesReceived);
            readBuf.resize(bytesReceived);
        }
        else
        {
            readBuf.clear();
        }

        return std::make_tuple(bytesReceived, std::move(readBuf));
    }

    void whenClientConnectionIsClosed()
    {
        m_connection.reset();
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
            m_connection->send(m_clientMessage.data(), m_clientMessage.size());

        m_sendResultQueue.push(
            bytesSent == (int) m_clientMessage.size()
            ? SystemError::noError
            : SystemError::getLastOSErrorCode());
    }

    void whenClientSentPingAsync(std::function<void()> auxiliaryHandler = nullptr)
    {
        whenClientSendsPingAsync(std::move(auxiliaryHandler));
        thenSendSucceeded();
    }

    void whenClientSendsPingAsync(std::function<void()> auxiliaryHandler = nullptr)
    {
        m_connection->sendAsync(
            &m_clientMessage,
            [this, auxiliaryHandler = std::move(auxiliaryHandler)](
                SystemError::ErrorCode systemErrorCode,
                std::size_t /*bytesSent*/)
            {
                if (auxiliaryHandler)
                    auxiliaryHandler();
                m_sendResultQueue.push(systemErrorCode);
            });
    }

    void thenSendSucceeded()
    {
        ASSERT_EQ(SystemError::noError, m_sendResultQueue.pop());
    }

    void thenClientSendTimesOutEventually()
    {
        thenSendFailedWith(SystemError::timedOut);
    }

    void thenSendFailedWith(SystemError::ErrorCode systemErrorCode)
    {
        ASSERT_EQ(systemErrorCode, m_sendResultQueue.pop());
    }

    void thenSendFailedWithUnrecoverableError()
    {
        const auto errorCode = m_sendResultQueue.pop();
        ASSERT_NE(SystemError::noError, errorCode);
        ASSERT_TRUE(socketCannotRecoverFromError(errorCode));
    }

    void whenClientSendsRandomDataSynchronouslyUntilFailure()
    {
        ASSERT_TRUE(m_connection->setNonBlockingMode(false));
        if (m_randomDataBuffer.empty())
            m_randomDataBuffer = nx::utils::generateRandomName(64 * 1024);

        for (int bytesSent = 0;
            (bytesSent = m_connection->send(m_randomDataBuffer.data(), m_randomDataBuffer.size())) > 0;)
        {
            m_sentData += m_randomDataBuffer.substr(0, bytesSent);
            m_randomDataBuffer = nx::utils::generateRandomName(64 * 1024);
        }

        m_sendResultQueue.push(SystemError::getLastOSErrorCode());
    }

    void whenClientSendsRandomDataAsyncNonStop()
    {
        ASSERT_TRUE(m_connection->setNonBlockingMode(true));
        if (m_randomDataBuffer.empty())
            m_randomDataBuffer = nx::utils::generateRandomName(64*1024);

        m_connection->sendAsync(
            &m_randomDataBuffer,
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
                &m_clientMessage,
                [this, connectionCtx = m_clientConnections.back().get()](
                    SystemError::ErrorCode errorCode,
                    std::size_t bytesTransferred)
                {
                    if (errorCode != SystemError::noError ||
                        bytesTransferred != m_clientMessage.size())
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
            << SystemError::getLastOSErrorText();

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

    void whenStartAcceptingConnections()
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

                this->whenStartAcceptingConnections();
            });
    }

    void whenAcceptConnectionAsync(std::function<void()> customHandler = nullptr)
    {
        m_serverSocket->acceptAsync(
            [this, customHandler = std::move(customHandler)](
                SystemError::ErrorCode systemErrorCode,
                std::unique_ptr<AbstractStreamSocket> connection)
            {
                if (customHandler)
                    customHandler();

                m_acceptedConnections.push(
                    std::make_tuple(systemErrorCode, std::move(connection)));
            });
    }

    void whenCancelAccept()
    {
        serverSocket()->cancelIOSync();
    }

    void whenStopServerSocket()
    {
        serverSocket()->pleaseStopSync();
    }

    void waitUntilConnectionIsAcceptedInNonBlockingMode()
    {
        ASSERT_TRUE(m_serverSocket->setNonBlockingMode(true))
            << SystemError::getLastOSErrorText();

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

                    // TODO: #akolesnikov Have to send something here because server socket with
                    // encryption auto-detection will not return socket until something is received.
                    // This condition should be handled by a more generic approach.
                    connectionCtx->connection.sendAsync(
                        &m_clientMessage,
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

    void thenAcceptFailed()
    {
        m_prevAcceptResult = m_acceptedConnections.pop();

        ASSERT_NE(SystemError::noError, std::get<0>(m_prevAcceptResult));
    }

    void thenConnectionHasBeenAccepted()
    {
        thenAcceptReported(SystemError::noError);

        ASSERT_NE(nullptr, std::get<1>(m_prevAcceptResult));
    }

    void thenNoConnectionCanBeAccepted()
    {
        auto connection = std::make_unique<typename SocketTypeSet::ClientSocket>();
        ASSERT_TRUE(connection->setNonBlockingMode(true));
        connection->connect(serverEndpoint(), nx::network::kNoTimeout);

        ASSERT_FALSE(m_acceptedConnections.pop(std::chrono::milliseconds(10)));
    }

    void thenConnectionIsEstablished()
    {
        ASSERT_EQ(SystemError::noError, m_connectResultQueue.pop());
    }

    void thenConnectionIsNotEstablished()
    {
        ASSERT_NE(SystemError::noError, m_connectResultQueue.pop());
    }

    void thenEveryConnectionEstablishedSuccessfully()
    {
        for (std::size_t i = 0; i < m_clientConnections.size(); ++i)
        {
            ASSERT_EQ(SystemError::noError, m_connectResultQueue.pop());
        }
    }

    void thenServerSocketReceivesAllDataBeforeEof()
    {
        thenConnectionHasBeenAccepted();

        assertAcceptedConnectionReceivedAllSentData();
        assertAcceptedConnectionReceivedEof();
    }

    static constexpr auto kDefaultTimerValue = std::chrono::milliseconds(10);

    int whenRegisterTimer(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
    {
        m_connection->registerTimer(
            timeout ? *timeout : kDefaultTimerValue,
            [this, seq = ++m_lastTimerSequence]() { m_timerShots.push(seq); });
        return m_lastTimerSequence;
    }

    void whenCancelTimer()
    {
        m_connection->cancelIOSync(aio::EventType::etTimedOut);
    }

    int thenTimerIsInvoked()
    {
        return m_timerShots.pop();
    }

    void thenTimerIsNotInvoked()
    {
        ASSERT_FALSE(m_timerShots.pop(kDefaultTimerValue * 3));
    }

    void assertAcceptedConnectionReceivedAllSentData()
    {
        assertAcceptedConnectionReceived(m_sentData);
    }

    void assertAcceptedConnectionReceived(const nx::Buffer& expected)
    {
        ASSERT_TRUE(lastAcceptedSocket()->setNonBlockingMode(false));

        whenServerReadsBytesWithFlags(expected.size(), 0);

        ASSERT_EQ(expected, m_synchronousServerReceivedData.internalBuffer());
        m_synchronousServerReceivedData = nx::utils::bstream::test::NotifyingOutput();
    }

    void assertAcceptedConnectionReceivedEof()
    {
        char buf[16];
        // Sometimes, connection will report connection break, not gaceful shutdown. But it's ok.
        int bytesRead = std::get<1>(m_prevAcceptResult)->recv(buf, sizeof(buf));
        ASSERT_LE(bytesRead, 0);
    }

    void assertAcceptedConnectionReportsNotConnected()
    {
        ASSERT_FALSE(std::get<1>(m_prevAcceptResult)->isConnected());
    }

    void assertClientConnectionReportsNotConnected()
    {
        ASSERT_FALSE(m_connection->isConnected());
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

    void thenServerMessageIsReceived(bool ignoreError = true)
    {
        for (;;)
        {
            const auto prevRecvResult = m_recvResultQueue.pop();
            if (ignoreError &&
                std::get<0>(prevRecvResult) != SystemError::noError)
            {
                continue;
            }

            ASSERT_EQ(SystemError::noError, std::get<0>(prevRecvResult));
            const auto& messageReceived = std::get<1>(prevRecvResult);
            ASSERT_TRUE(messageReceived.starts_with(m_serverMessage));
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

    void thenSocketCanBeUsedForAsyncIo()
    {
        this->whenClientSentPingAsync();

        this->thenServerMessageIsReceived();
    }

    void thenEveryConnectionIsAccepted()
    {
        for (int i = 0; i < (int) m_clientConnections.size(); ++i)
        {
            ASSERT_NE(nullptr, std::get<1>(m_acceptedConnections.pop()));
        }
    }

    void assertConnectionCanDoSyncIo()
    {
        ASSERT_TRUE(connection()->setNonBlockingMode(false));

        whenClientSentPing();

        whenReadSocketInBlockingWay();
        thenServerMessageIsReceived(false);
    }

    void assertConnectionCanDoAsyncIo()
    {
        ASSERT_TRUE(connection()->setNonBlockingMode(true));

        whenClientSendsPingAsync();
        thenSendSucceeded();

        startReadingConnectionAsync();
        thenServerMessageIsReceived(false);
    }

    void doSyncIoUntilFirstFailure()
    {
        ASSERT_TRUE(connection()->setNonBlockingMode(false));

        doIoUntilFirstFailure(
            std::bind(&StreamSocketAcceptance::whenClientSendsPing, this),
            std::bind(&StreamSocketAcceptance::whenReadSocketInBlockingWay, this));
    }

    void doAsyncIoUntilFirstFailure()
    {
        ASSERT_TRUE(connection()->setNonBlockingMode(true));

        doIoUntilFirstFailure(
            [this](auto&&... args) { whenClientSendsPingAsync(std::move(args)...); },
            [this](auto&&... args) { startReadingConnectionAsync(std::move(args)...); });
    }

    template<typename SendPingFunc, typename RecvPingFunc>
    void doIoUntilFirstFailure(
        SendPingFunc sendPingFunc,
        RecvPingFunc recvPingFunc)
    {
        sendPingFunc();
        if (m_sendResultQueue.pop() != SystemError::noError)
            return;

        recvPingFunc();

        const auto prevRecvResult = m_recvResultQueue.pop();

        if (std::get<0>(prevRecvResult) != SystemError::noError ||
            std::get<1>(prevRecvResult).empty()) //< Connection closed.
        {
            return;
        }

        const auto& messageReceived = std::get<1>(prevRecvResult);
        ASSERT_TRUE(messageReceived.starts_with(m_serverMessage));
    }

    //---------------------------------------------------------------------------------------------

    template<typename T>
    SocketAddress getServerEndpointForConnectShutdown(
        typename std::enable_if_t<
            std::is_same<
                typename std::remove_const_t<decltype(T::serverEndpointForConnectShutdown)>,
                SocketAddress>::value>* = nullptr)
    {
        return T::serverEndpointForConnectShutdown;
    }

    template<typename T>
    SocketAddress getServerEndpointForConnectShutdown(...)
    {
        return serverEndpoint();
    }

    bool hasLimitation(SocketTypeLimitation limitation) const
    {
        return hasLimitationImpl<SocketTypeSet>(limitation);
    }

    //---------------------------------------------------------------------------------------------

    void givenConnectionBlockedInConnect()
    {
        givenSilentServer();

        auto connectResultQueue = std::make_shared<nx::utils::SyncQueue<bool>>();

        m_clientSocketThread = std::thread(
            [this, connectResultQueue]()
            {
                NX_MUTEX_LOCKER lock(&this->m_mutex);

                for (;;)
                {
                    m_connection = std::make_unique<typename SocketTypeSet::ClientSocket>();

                    nx::Unlocker<nx::Mutex> unlock(&lock);

                    const auto connectResult = m_connection->connect(
                        getServerEndpointForConnectShutdown<SocketTypeSet>(nullptr),
                        nx::network::kNoTimeout);

                    if (!connectResult)
                        break; //< Assuming that socket has been shutdown.
                    connectResultQueue->push(connectResult);
                }
            });

        for (;;)
        {
            const auto connectResult = connectResultQueue->pop(std::chrono::milliseconds(100));
            if (!connectResult)
                break; //< Assuming that connect has blocked.
            ASSERT_TRUE(*connectResult);
        }
    }

    void givenConnectionBlockedInSend()
    {
        givenSilentServer();
        givenConnectedSocket();

        auto sendResultQueue = std::make_shared<nx::utils::SyncQueue<int>>();

        m_clientSocketThread = std::thread(
            [this, sendResultQueue]()
            {
                std::array<char, 16*1024> sendBuffer;
                for (;;)
                {
                    // This send will block eventually.
                    const int bytesSent = m_connection->send(
                        sendBuffer.data(), sendBuffer.size());
                    if (bytesSent <= 0)
                        break; //< Assuming that socket has been shutdown.
                    sendResultQueue->push(bytesSent);
                }
            });

        for (;;)
        {
            const auto bytesSent = sendResultQueue->pop(std::chrono::milliseconds(100));
            if (!bytesSent)
            {
                // Assuming that send has blocked.
                // There is no way to reliably check that m_clientSocketThread is blocked in send,
                // but it will happen at least sometimes, so this test will catch
                // an existing problem (if any) with number of runs -> infinity.
                break;
            }
            ASSERT_GT(*bytesSent, 0);
        }
    }

    void givenConnectionBlockedInRecv()
    {
        givenSilentServer();
        givenConnectedSocket();

        m_clientSocketThread = std::thread(
            [this]()
            {
                char buf[16];
                m_connection->recv(buf, sizeof(buf), 0);
            });
    }

    void whenInvokeShutdown()
    {
        NX_MUTEX_LOCKER lock(&this->m_mutex);

        m_connection->shutdown();
    }

    void whenInvokeShutdownInTimer()
    {
        NX_MUTEX_LOCKER lock(&this->m_mutex);

        std::promise<void> promise;
        nx::network::aio::Timer timer;
        timer.start(std::chrono::milliseconds(1),
            [this, &promise]()
            {
                m_connection->shutdown();
                promise.set_value();
            });
        promise.get_future().wait();
        timer.pleaseStopSync();
    }

    void thenConnectionOperationIsInterrupted()
    {
        m_clientSocketThread.join();
    }

    void thenPollableIsStillValid()
    {
        ASSERT_NE(nullptr, m_connection->pollable());
    }

    //---------------------------------------------------------------------------------------------

    void whenSendDataConcurrentlyThroughConnectedSockets()
    {
        m_sentData = nx::utils::generateRandomName(1024 * 1024);

        givenListeningServerSocket();
        whenStartAcceptingConnections();

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

    std::unique_ptr<typename SocketTypeSet::ServerSocket> createServerSocket()
    {
        return std::make_unique<typename SocketTypeSet::ServerSocket>();
    }

    typename SocketTypeSet::ClientSocket* connection()
    {
        return m_connection.get();
    }

    std::unique_ptr<typename SocketTypeSet::ClientSocket> takeConnection()
    {
        return std::exchange(m_connection, {});
    }

    typename SocketTypeSet::ServerSocket* serverSocket()
    {
        return m_serverSocket.get();
    }

    AbstractStreamSocket* lastAcceptedSocket()
    {
        return std::get<1>(m_prevAcceptResult).get();
    }

    void freeServerSocket()
    {
        m_serverSocket.reset();
    }

    const nx::Buffer& clientMessage() const
    {
        return m_clientMessage;
    }

private:
    template<typename SocketTypeSet_>
    static bool hasLimitationImpl(
        SocketTypeLimitation value,
        typename std::enable_if_t<std::is_same_v<
            std::void_t<decltype(SocketTypeSet_::limitations)>, void>>* = nullptr)
    {
        for (auto c: SocketTypeSet::limitations)
        {
            if (c == value)
                return true;
        }

        return false;
    }

    template<typename>
    static bool hasLimitationImpl(...)
    {
        // If SocketTypeSet does not specify limitations, then assuming everything supported.
        return false;
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
    std::thread m_clientSocketThread;
    int m_lastTimerSequence = 0;
    nx::utils::SyncQueue<int /*timer sequence*/> m_timerShots;

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
            while (!received.starts_with(expected))
                received += dataReceivedQueue.pop();
        }
    };

    std::vector<std::unique_ptr<ConcurrentSocketPipeContext>> m_concurrentSocketPipes;

    //---------------------------------------------------------------------------------------------

    std::unique_ptr<BasicSynchronousReceivingServer> m_synchronousServer;
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
};

TYPED_TEST_SUITE_P(StreamSocketAcceptance);

//-------------------------------------------------------------------------------------------------

// Windows 8/10 does not support client->server send timeout.
// It closes connection automatically with error 10053 after client send timeout.
TYPED_TEST_P(StreamSocketAcceptance, DISABLED_receiveDelay)
{
    this->runStreamingTest({.doServerDelay = true, .doClientDelay = false, .assertOnError = true});
}

TYPED_TEST_P(StreamSocketAcceptance, sendDelay)
{
    // NOTE: there is a weird trouble with this test on win32 that results in WSA_IO_PENDING error
    // reported by recv which leads to the test failure.
    // This situation is unexpected and can be a bug in a network driver.
    // So, working around the issue by retrying test multiple times.

    static constexpr int kTries = 11;
    for (int i = 0; i < kTries; ++i)
    {
        if (this->runStreamingTest({.doServerDelay = false, .doClientDelay = true, .assertOnError = false}))
            return;
    }

    FAIL() << "The test did not pass in "<< kTries << " tries";
}

//-------------------------------------------------------------------------------------------------
// Connect tests.

TYPED_TEST_P(StreamSocketAcceptance, connect_uses_address_resolver)
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

TYPED_TEST_P(StreamSocketAcceptance, async_connect_is_cancelled_by_cancelling_write)
{
    this->givenSocketInConnectStage();

    this->connection()->cancelIOSync(aio::etWrite);

    this->thenSocketCanBeSafelyRemoved();
}

TYPED_TEST_P(StreamSocketAcceptance, async_connect_is_cancelled_by_pleaseStopSync)
{
    this->givenSocketInConnectStage();

    this->connection()->pleaseStopSync();

    this->thenSocketCanBeSafelyRemoved();
}

//---------------------------------------------------------------------------------------------
// I/O data transfer tests.

TYPED_TEST_P(StreamSocketAcceptance, transfer_async)
{
    constexpr int connectionCount = 7;

    this->givenPingPongServer();
    this->whenSendMultiplePingsViaMultipleConnections(connectionCount);
    this->thenPongIsReceivedViaEachConnection();
}

TYPED_TEST_P(StreamSocketAcceptance, synchronous_server_receives_data)
{
    this->givenListeningSynchronousServer();
    this->givenConnectedSocket();

    this->whenSendRandomDataToServer();

    this->thenServerReceivedData();
}

TYPED_TEST_P(StreamSocketAcceptance, synchronous_server_responds_to_request)
{
    this->givenSynchronousPingPongServer();
    this->givenConnectedSocket();

    this->whenClientSentPing();

    this->whenReadSocketInBlockingWay();
    this->thenServerMessageIsReceived();
}

TYPED_TEST_P(StreamSocketAcceptance, recv_sync_with_wait_all_flag)
{
    this->givenListeningServerSocket();
    this->givenConnectedSocket();

    this->whenSendAsyncRandomDataToServer();
    this->whenServerReadsWithFlags(MSG_WAITALL);

    this->thenServerReceivedData();
}

TYPED_TEST_P(StreamSocketAcceptance, recv_timeout_is_reported)
{
    this->givenSilentServer();
    this->givenConnectedSocket();
    this->setClientSocketRecvTimeout(std::chrono::milliseconds(1));

    this->whenReadSocketInBlockingWay();

    this->thenClientSocketReportedTimedout();
}

TYPED_TEST_P(StreamSocketAcceptance, msg_dont_wait_flag_makes_recv_call_nonblocking)
{
    this->givenPingPongServer();
    this->givenConnectedSocket();

    this->whenReadSocketInBlockingWayWithFlags(MSG_DONTWAIT);
    this->thenClientSocketReported(SystemError::wouldBlock);

    this->whenClientSendsPing();
    this->whenReadSocketInBlockingWay();
    this->thenServerMessageIsReceived();
}

TYPED_TEST_P(StreamSocketAcceptance, concurrent_recv_send_in_blocking_mode)
{
    if (this->hasLimitation(SocketTypeLimitation::noConcurrentBlockingIo))
        GTEST_SKIP() << "Current socket type does not support concurrent blocking I/O";

    this->whenSendDataConcurrentlyThroughConnectedSockets();
    this->thenBothSocketsReceiveExpectedData();
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

TYPED_TEST_P(StreamSocketAcceptance, DISABLED_socket_is_reusable_after_send_timeout)
{
    if (this->hasLimitation(SocketTypeLimitation::notUsableAfterSendInterrupt))
        GTEST_SKIP() << "Current socket type cannot be used further after send timeout";

    this->givenClientConnectionTimedOutOnSend();

    // Receiving data sent before timedOut.
    this->thenConnectionHasBeenAccepted();
    this->assertAcceptedConnectionReceivedAllSentData();

    // Verifying that socket can be used after timedOut.
    this->whenSendRandomDataToServer();
    this->assertAcceptedConnectionReceivedAllSentData();
}

TYPED_TEST_P(StreamSocketAcceptance, sync_send_reports_timedOut)
{
    this->givenAcceptingServerSocket();
    this->givenConnectedSocket();
    this->setClientSocketSendTimeout(std::chrono::milliseconds(1));

    this->whenClientSendsRandomDataSynchronouslyUntilFailure();

    this->thenClientSendTimesOutEventually();
}

TYPED_TEST_P(StreamSocketAcceptance, async_send_reports_timedOut)
{
    this->givenAcceptingServerSocket();
    this->givenConnectedSocket();
    this->setClientSocketSendTimeout(std::chrono::milliseconds(1));

    this->whenClientSendsRandomDataAsyncNonStop();

    this->thenClientSendTimesOutEventually();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    all_data_sent_is_received_after_remote_end_closed_connection)
{
    this->givenAcceptingServerSocket();
    this->givenConnectedSocket();

    auto expectedData = this->whenSendRandomDataToServer();

    // Ensuring that the SSL handshake is completed before the client connection is closed.
    // Otherwise, the accepted connection may not be able to read (depends on SSL version).
    this->thenConnectionHasBeenAccepted();
    this->assertAcceptedConnectionReceived(expectedData.substr(0, 1));
    expectedData = expectedData.substr(1);

    this->whenClientConnectionIsClosed();

    this->assertAcceptedConnectionReceived(expectedData);
    this->assertAcceptedConnectionReceivedEof();
    this->assertAcceptedConnectionReportsNotConnected();
}

//---------------------------------------------------------------------------------------------
// I/O cancellation tests.

TYPED_TEST_P(StreamSocketAcceptance, randomly_stopping_multiple_simultaneous_connections)
{
    this->givenListeningServerSocket(kTotalConnections);
    this->givenRandomNameMappedToServerHostIp();

    this->startMaximumConcurrentConnections(this->mappedEndpoint());

    // TODO: #akolesnikov Refactor this test

    int canCancelIndex = 0;
    int cancelledConnectionsCount = 0;
    NX_MUTEX_LOCKER lk(&this->m_mutex);
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
        lk.relock();
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
}

TYPED_TEST_P(StreamSocketAcceptance, socket_removed_while_receiving_data)
{
    this->givenSpamServer();
    this->givenConnectedSocket();

    this->whenClientSentPing();
    // The server starts sending random data.
    this->whenReadSocketInBlockingWay();

    this->whenClientConnectionIsClosed();

    // then server has received an error and can be freed.
}

// TODO: #akolesnikov Modify and uncomment this test.
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

TYPED_TEST_P(StreamSocketAcceptance, socket_is_ready_for_io_after_read_cancellation)
{
    this->givenPingPongServer();
    this->givenConnectedSocket();

    ASSERT_TRUE(this->connection()->setNonBlockingMode(true));
    this->startReadingConnectionAsync();
    this->connection()->cancelIOSync(aio::etRead);

    this->assertConnectionCanDoSyncIo();
    this->assertConnectionCanDoAsyncIo();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    socket_is_usable_after_exception_is_thrown_by_read_completion_handler)
{
    this->givenPingPongServer();
    this->givenConnectedSocket();

    this->whenClientSentPing();
    this->whenReceivedMessageFromServerAsync([]() { throw std::runtime_error("test"); });

    this->assertConnectionCanDoSyncIo();
    this->assertConnectionCanDoAsyncIo();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    exception_thrown_by_recv_handler_is_handled_properly_even_when_socket_was_removed)
{
    this->givenPingPongServer();
    this->givenConnectedSocket();

    this->whenClientSentPing();

    std::promise<void> done;
    this->whenReceivedMessageFromServerAsync(
        [this, &done]()
        {
            this->whenClientConnectionIsClosed();

            // Making sure we signal the promise after this handler completion.
            SocketGlobals::instance().aioService().getCurrentAioThread()->post(
                nullptr, [&done]() { done.set_value(); });

            throw std::runtime_error("test");
        });

    done.get_future().wait();

    // assert process does not crash.
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    socket_is_usable_after_exception_is_thrown_by_send_completion_handler)
{
    this->givenListeningServerSocket();
    this->givenConnectedSocket();

    this->whenSendAsyncRandomDataToServer([]() { throw std::runtime_error("test"); });
    this->whenServerReadsWithFlags(MSG_WAITALL);

    this->thenServerReceivedData();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    exception_thrown_by_send_handler_is_handled_properly_even_when_socket_was_removed)
{
    this->givenListeningServerSocket();
    this->givenConnectedSocket();

    std::promise<void> done;

    this->whenSendAsyncRandomDataToServer(
        [this, &done]()
        {
            this->whenClientConnectionIsClosed();

            // Making sure we signal the promise after this handler completion.
            SocketGlobals::instance().aioService().getCurrentAioThread()->post(
                nullptr, [&done]() { done.set_value(); });

            throw std::runtime_error("test");
        });

    done.get_future().wait();

    this->whenAcceptConnection();
    this->thenConnectionHasBeenAccepted();
    this->whenServerReadsSome();

    // assert process does not crash.
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    socket_aio_thread_can_be_changed_after_io_cancellation_during_connect_completion)
{
    this->givenPingPongServer();

    this->whenConnectToServer(
        [this]()
        {
            this->connection()->cancelIOSync(aio::etNone);
            this->connection()->bindToAioThread(
                SocketGlobals::aioService().getRandomAioThread());

            this->startReadingConnectionAsync();
        });

    this->thenSocketCanBeUsedForAsyncIo();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    socket_aio_thread_can_be_changed_after_io_cancellation_during_send_completion)
{
    this->givenPingPongServer();
    this->givenConnectedSocket();
    ASSERT_TRUE(this->connection()->setNonBlockingMode(true));
    this->startReadingConnectionAsync();

    this->whenClientSentPingAsync(
        [this]()
        {
            this->connection()->cancelIOSync(aio::etNone);
            this->connection()->bindToAioThread(
                SocketGlobals::aioService().getRandomAioThread());

            this->startReadingConnectionAsync();
        });

    this->thenServerMessageIsReceived();
    this->thenSocketCanBeUsedForAsyncIo();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    change_aio_thread_of_accepted_connection)
{
    this->givenPingPongServer();

    this->givenConnectedSocket();
    this->startReadingConnectionAsync();
    this->whenClientSentPingAsync();

    this->thenServerMessageIsReceived();
}

TYPED_TEST_P(StreamSocketAcceptance, socket_is_usable_after_send_cancellation)
{
    if (this->hasLimitation(SocketTypeLimitation::notUsableAfterSendInterrupt))
        GTEST_SKIP() << "Current socket type cannot be used further after send cancellation";

    // SSL socket cannot recover from incomplete send.
    // So, it can be ready for I/O or can report an error.
    // Checking that it does not crash at least.

    this->givenPingPongServer();

    for (int i = 0; i < 2; ++i)
    {
        this->givenConnectedSocket();

        ASSERT_TRUE(this->connection()->setNonBlockingMode(true));
        this->whenClientSendsPingAsync();
        this->connection()->cancelIOSync(aio::etWrite);

        if (i == 0)
            this->doSyncIoUntilFirstFailure();
        else
            this->doAsyncIoUntilFirstFailure();
    }
}

TYPED_TEST_P(StreamSocketAcceptance, DISABLED_shutdown_interrupts_connect)
{
    this->givenConnectionBlockedInConnect();
    this->whenInvokeShutdown();
    this->thenConnectionOperationIsInterrupted();
}

TYPED_TEST_P(StreamSocketAcceptance, shutdown_interrupts_send)
{
    this->givenConnectionBlockedInSend();
    this->whenInvokeShutdown();
    this->thenConnectionOperationIsInterrupted();
}

TYPED_TEST_P(StreamSocketAcceptance, shutdown_interrupts_recv)
{
    this->givenConnectionBlockedInRecv();
    this->whenInvokeShutdown();
    this->thenConnectionOperationIsInterrupted();
}

TYPED_TEST_P(StreamSocketAcceptance, shutdown_in_timer_interrupts_recv)
{
    this->givenConnectionBlockedInRecv();
    this->whenInvokeShutdownInTimer();
    this->thenConnectionOperationIsInterrupted();
}

TYPED_TEST_P(StreamSocketAcceptance, pollable_is_valid_after_shutdown)
{
    this->givenSilentServer();
    this->givenConnectedSocket();

    this->whenInvokeShutdown();

    this->thenPollableIsStillValid();
}

//-------------------------------------------------------------------------------------------------
// Accepting side tests.

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
    constexpr int connectionCount = 11;

    this->givenAcceptingServerSocket();
    this->whenEstablishMultipleConnectionsAsync(connectionCount);
    this->thenEveryConnectionIsAccepted();
}

TYPED_TEST_P(
    StreamSocketAcceptance,
    server_socket_accepts_multiple_connections_found_in_socket_backlog)
{
    constexpr int connectionCount = 11;

    this->givenListeningServerSocket();

    this->whenEstablishMultipleConnectionsAsync(connectionCount);
    this->whenStartAcceptingConnections();

    this->thenEveryConnectionIsAccepted();
}

// TODO: #akolesnikov Following test is not relevant for Macosx since server socket there
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

TYPED_TEST_P(StreamSocketAcceptance, server_socket_accept_times_out)
{
    this->givenListeningServerSocket();
    this->setServerSocketAcceptTimeout(std::chrono::milliseconds(1));

    this->whenAcceptConnection();

    this->thenAcceptReported(SystemError::timedOut);
}

TYPED_TEST_P(StreamSocketAcceptance, server_socket_accept_async_times_out)
{
    this->givenListeningNonBlockingServerSocket();
    this->setServerSocketAcceptTimeout(std::chrono::milliseconds(1));

    this->whenAcceptConnectionAsync();

    this->thenAcceptReported(SystemError::timedOut);
}

TYPED_TEST_P(StreamSocketAcceptance, accept_async_on_blocking_socket_results_in_error)
{
    this->givenListeningServerSocket();
    this->whenAcceptConnectionAsync();
    this->thenAcceptFailed();
}

// TODO: #akolesnikov Delete or enable in NXLIB-338.
TYPED_TEST_P(StreamSocketAcceptance, DISABLED_server_socket_can_be_freed_in_accept_handler)
{
    this->givenListeningNonBlockingServerSocket();
    this->setServerSocketAcceptTimeout(std::chrono::milliseconds(1));

    this->whenAcceptConnectionAsync(
        [this]()
        {
            this->freeServerSocket();
        });

    this->thenAcceptReported(SystemError::timedOut);
}

TYPED_TEST_P(StreamSocketAcceptance, accept_is_cancelled_with_cancelIo)
{
    this->givenAcceptingServerSocket();
    this->whenCancelAccept();
    this->thenNoConnectionCanBeAccepted();
}

TYPED_TEST_P(StreamSocketAcceptance, accept_is_cancelled_with_pleaseStop)
{
    this->givenAcceptingServerSocket();
    this->whenStopServerSocket();
    this->thenNoConnectionCanBeAccepted();
}

TYPED_TEST_P(StreamSocketAcceptance, reuse_addr)
{
    auto server1 = this->createServerSocket();
    ASSERT_TRUE(server1->setReuseAddrFlag(true));
    ASSERT_TRUE(server1->bind(SocketAddress::anyAddress));

    auto server2 = this->createServerSocket();
    ASSERT_TRUE(server2->setReuseAddrFlag(true));
    ASSERT_TRUE(server2->bind(
        SocketAddress(HostAddress::localhost, server1->getLocalAddress().port)));
}

TYPED_TEST_P(StreamSocketAcceptance, reuse_port)
{
    auto server1 = this->createServerSocket();
    if (!server1->setReusePortFlag(true))
    {
        ASSERT_EQ(SystemError::unknownProtocolOption, SystemError::getLastOSErrorCode());
        std::cout << "SO_REUSEPORT is unsupported. Skipping this test" << std::endl;
        return;
    }
    ASSERT_TRUE(server1->bind(SocketAddress::anyPrivateAddress));

    auto server2 = this->createServerSocket();
    ASSERT_TRUE(server2->setReusePortFlag(true));
    ASSERT_TRUE(server2->bind(server1->getLocalAddress()));
}

TYPED_TEST_P(StreamSocketAcceptance, timer_works)
{
    this->givenClientSocket();
    this->whenRegisterTimer();
    this->thenTimerIsInvoked();
}

TYPED_TEST_P(StreamSocketAcceptance, timer_can_be_cancelled)
{
    this->givenClientSocket();

    std::promise<void> done;
    this->connection()->post(
        [this, &done]()
        {
            this->whenRegisterTimer();
            this->whenCancelTimer();
            done.set_value();
        });
    done.get_future().wait();

    this->thenTimerIsNotInvoked();
}

TYPED_TEST_P(StreamSocketAcceptance, existing_timer_can_be_changed)
{
    this->givenClientSocket();

    std::promise<void> done;
    this->connection()->post(
        [this, &done]()
        {
            this->whenRegisterTimer(std::chrono::hours(24));
            this->whenRegisterTimer(std::chrono::milliseconds(10));
            done.set_value();
        });
    done.get_future().wait();

    this->thenTimerIsInvoked();
}

TYPED_TEST_P(StreamSocketAcceptance, timer_can_be_rescheduled)
{
    using namespace std::chrono;

    static constexpr auto kDelay = std::chrono::milliseconds(200);
    static constexpr auto kRescheduleCount = 3;

    // NOTE: In general case timers cannot be expected to shoot in time. They can shoot later.
    // But, an early timer shot should not happen and is considered to be a bug.

    this->givenClientSocket();

    for (int i = 0; i < kRescheduleCount; ++i)
    {
        this->whenRegisterTimer(kDelay);
        std::this_thread::sleep_for(kDelay / (kRescheduleCount + 1));
    }

    const auto timerScheduleTime = steady_clock::now();
    const auto expectedTimerSequence = this->whenRegisterTimer(kDelay);

    while (this->thenTimerIsInvoked() != expectedTimerSequence) {}

    // Early timer shot means that at least one of previous timers shots was not cancelled.
    // Rounding up to compensate precision errors in nanosecond-resolution clock.
    ASSERT_GE(
        ceil<milliseconds>(steady_clock::now() - timerScheduleTime),
        kDelay);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(StreamSocketAcceptance);
REGISTER_TYPED_TEST_SUITE_P(StreamSocketAcceptance,
    DISABLED_receiveDelay,
    sendDelay,

    //---------------------------------------------------------------------------------------------
    // Connect tests.
    connect_uses_address_resolver,
    connect_including_resolve_is_cancelled_correctly,
    connect_including_resolving_unknown_name_is_cancelled_correctly,
    async_connect_is_cancelled_by_cancelling_write,
    async_connect_is_cancelled_by_pleaseStopSync,

    //---------------------------------------------------------------------------------------------
    // I/O data transfer tests.
    transfer_async,
    synchronous_server_receives_data,
    synchronous_server_responds_to_request,
    recv_sync_with_wait_all_flag,
    recv_timeout_is_reported,
    msg_dont_wait_flag_makes_recv_call_nonblocking,
    concurrent_recv_send_in_blocking_mode,
    socket_is_reusable_after_recv_timeout,

    // This test doesn't work for SSL connections because SSL_write doesn't allow to send different
    // data after SSL_ERROR_WANT_WRITE.
    DISABLED_socket_is_reusable_after_send_timeout,

    sync_send_reports_timedOut,
    async_send_reports_timedOut,
    all_data_sent_is_received_after_remote_end_closed_connection,

    //---------------------------------------------------------------------------------------------
    // I/O cancellation tests.
    randomly_stopping_multiple_simultaneous_connections,
    receive_timeout_change_is_not_ignored,
    socket_removed_while_receiving_data,
    cancel_io,
    socket_is_ready_for_io_after_read_cancellation,
    socket_is_usable_after_exception_is_thrown_by_read_completion_handler,
    exception_thrown_by_recv_handler_is_handled_properly_even_when_socket_was_removed,
    socket_is_usable_after_exception_is_thrown_by_send_completion_handler,
    exception_thrown_by_send_handler_is_handled_properly_even_when_socket_was_removed,
    socket_aio_thread_can_be_changed_after_io_cancellation_during_connect_completion,
    socket_aio_thread_can_be_changed_after_io_cancellation_during_send_completion,
    change_aio_thread_of_accepted_connection,
    socket_is_usable_after_send_cancellation,

    // This test is disabled because currently it does not work on Linux and Mac. In future, we may
    // introduce connect implementation with shutdown support.
    DISABLED_shutdown_interrupts_connect,

    shutdown_interrupts_send,
    shutdown_interrupts_recv,
    shutdown_in_timer_interrupts_recv,
    pollable_is_valid_after_shutdown,

    //---------------------------------------------------------------------------------------------
    // Accepting side tests.
    nonblocking_accept_reports_wouldBlock_if_no_incoming_connections,
    nonblocking_accept_actually_accepts_connections,
    accepted_socket_is_in_blocking_mode_when_server_socket_is_nonblocking,
    accepted_socket_is_in_blocking_mode_when_server_socket_is_blocking,
    server_socket_accepts_many_connections_in_a_row,
    server_socket_accepts_multiple_connections_found_in_socket_backlog,
    DISABLED_server_socket_listen_queue_size_is_used,
    server_socket_accept_times_out,
    server_socket_accept_async_times_out,
    accept_async_on_blocking_socket_results_in_error,
    DISABLED_server_socket_can_be_freed_in_accept_handler,

    accept_is_cancelled_with_cancelIo,
    accept_is_cancelled_with_pleaseStop,

    //---------------------------------------------------------------------------------------------
    // Socket options.
    reuse_addr,
    reuse_port,

    //---------------------------------------------------------------------------------------------
    // Socket timer (AbstractCommunicatingSocket::registerTimer).
    // Not to be confused with recv/send timeouts.
    timer_works,
    timer_can_be_cancelled,
    timer_can_be_rescheduled,
    existing_timer_can_be_changed
);

} // namespace test
} // namespace network
} // namespace nx
