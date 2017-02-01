#pragma once

#include <iostream>

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/test_support/utils.h>
#include <utils/common/guard.h>
#include <utils/common/stoppable.h>
#include <utils/common/systemerror.h>

namespace nx {
namespace network {
namespace test {

namespace {

const QByteArray kTestMessage("Ping");
const std::chrono::milliseconds kTestTimeout(5000);
static size_t testClientCount() { return nx::utils::TestOptions::applyLoadMode<size_t>(10); }
static std::string lastError() { return SystemError::getLastOSErrorText().toStdString(); }

const bool kEnableTestDebugOutput = false;
static void testDebugOutput(const QnLogMessage& message)
{
    if (kEnableTestDebugOutput)
        NX_LOG(lm("nx::network::test: %1").arg(message), cl_logDEBUG1);
}

} // namespace

template<typename SocketType>
QByteArray readNBytes(SocketType* clientSocket, int count)
{
    const int BUF_SIZE = std::max<int>(count, 128);

    QByteArray buffer(BUF_SIZE, char(0));
    int bufDataSize = 0;
    for (;;)
    {
        const auto bytesRead = clientSocket->recv(
            buffer.data() + bufDataSize,
            buffer.size() - bufDataSize,
            0);
        if (bytesRead <= 0)
            return buffer.mid(0, bufDataSize);

        bufDataSize += bytesRead;
        if (bufDataSize >= count)
            return buffer.mid(0, bufDataSize);
    }
}


enum class ErrorHandling
{
    ignore,
    triggerAssert
};

template<typename ServerSocketType>
class SyncSocketServer
{
public:
    struct ServerStartResult
    {
        bool hasSucceeded;
        SocketAddress address;

        ServerStartResult(
            bool hasSucceeded,
            SocketAddress address)
            :
            hasSucceeded(hasSucceeded),
            address(std::move(address))
        {
        }
    };

    SyncSocketServer(ServerSocketType server):
        m_server(std::move(server))
    {
    }

    void setEndpointToBindTo(SocketAddress endpoint)
    {
        m_endpointToBindTo = std::move(endpoint);
    }

    void setTestMessage(Buffer value)
    {
        m_testMessage = std::move(value);
    }

    void setErrorHandling(ErrorHandling value)
    {
        m_errorHandling = value;
    }

    SocketAddress start()
    {
        m_thread = std::make_unique<nx::utils::thread>([this](){ run(); });
        const auto result = m_startedPromise.get_future().get();
        NX_GTEST_ASSERT_TRUE(result.hasSucceeded);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return result.address;
    }

    ~SyncSocketServer()
    {
        m_needToStop = true;
        if (m_thread)
            m_thread->join();
    }

private:
    void run()
    {
        auto startedPromiseGuard = makeScopedGuard(
            [this]() { m_startedPromise.set_value(ServerStartResult(false, SocketAddress())); });

        ASSERT_TRUE(m_server->setReuseAddrFlag(true)) << lastError();
        ASSERT_TRUE(m_server->bind(m_endpointToBindTo)) << lastError();
        ASSERT_TRUE(m_server->listen(testClientCount())) << lastError();

        ASSERT_TRUE(m_server->setRecvTimeout(100)) << lastError();
        std::unique_ptr<AbstractStreamSocket> client(m_server->accept());
        ASSERT_FALSE(client);
        ASSERT_EQ(SystemError::timedOut, SystemError::getLastOSErrorCode());

        auto serverAddress = m_server->getLocalAddress();

        NX_LOGX(lm("Started on %1").arg(serverAddress.toString()), cl_logINFO);
        startedPromiseGuard.disarm();
        m_startedPromise.set_value(ServerStartResult(true, std::move(serverAddress)));

        const auto startTime = std::chrono::steady_clock::now();
        const auto maxTimeout = kTestTimeout * testClientCount();
        while (!m_needToStop)
        {
            std::unique_ptr<AbstractStreamSocket> client(m_server->accept());
            if (!client)
            {
                if (SystemError::getLastOSErrorCode() == SystemError::timedOut ||
                    m_errorHandling == ErrorHandling::ignore)
                {
                    const auto runTime = std::chrono::steady_clock::now() - startTime;
                    NX_ASSERT( runTime < maxTimeout, "Are we frozen?");
                    continue;
                }
            }

            ASSERT_TRUE(client.get()) << lastError();
            ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
            ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

            if (m_testMessage.isEmpty())
                continue;

            const auto incomingMessage = readNBytes(client.get(), m_testMessage.size());
            if (m_errorHandling == ErrorHandling::triggerAssert)
            {
                ASSERT_TRUE(!incomingMessage.isEmpty()) << lastError();
                ASSERT_EQ(m_testMessage, incomingMessage);
            }

            const int bytesSent = client->send(m_testMessage);
            if (m_errorHandling == ErrorHandling::triggerAssert)
                ASSERT_NE(-1, bytesSent) << lastError();

            //waiting for connection to be closed by client
            QByteArray buf(64, 0);
            for (;;)
            {
                int bytesRead = client->recv(buf.data(), buf.size());
                if (bytesRead == 0)
                    break;
                const auto errorCode = SystemError::getLastOSErrorCode();
                if (bytesRead == -1 &&
                    errorCode != SystemError::timedOut &&
                    errorCode != SystemError::interrupted)
                {
                    break;
                }
            }
        }
    }

    ServerSocketType m_server;
    SocketAddress m_endpointToBindTo = SocketAddress::anyPrivateAddress;
    Buffer m_testMessage = kTestMessage;
    nx::utils::promise<ServerStartResult> m_startedPromise;
    ErrorHandling m_errorHandling{ErrorHandling::triggerAssert};
    std::atomic<bool> m_needToStop{false};
    std::unique_ptr<nx::utils::thread> m_thread;
};

template<typename Socket, typename ... Args>
std::unique_ptr<SyncSocketServer<Socket>> syncSocketServer(Socket socket, Args ... args)
{
    return std::make_unique<SyncSocketServer<Socket>>(
        std::move(socket), std::forward<Args>(args) ...);
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleSync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none,
    const QByteArray& testMessage = kTestMessage)
{
    const auto syncServer = syncSocketServer(serverMaker());
    auto serverAddress = syncServer->start();
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    nx::utils::thread clientThread(
        [endpointToConnectTo, &testMessage, &clientMaker]()
        {
            const auto clientCount = testClientCount();
            for (size_t i = 0; i != clientCount; ++i)
            {
                auto client = clientMaker();
                EXPECT_TRUE(client->connect(*endpointToConnectTo, kTestTimeout.count()))
                    << i << ": " << lastError();

                ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
                ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

                EXPECT_EQ(
                    testMessage.size(),
                    client->send(testMessage.constData(), testMessage.size())) << lastError();

                const auto incomingMessage = readNBytes(
                    client.get(), testMessage.size());

                ASSERT_TRUE(!incomingMessage.isEmpty()) << i << ": " << lastError();
                ASSERT_EQ(testMessage, incomingMessage);
                client.reset();
            }
        });

    clientThread.join();
}

static void testWouldBlockLastError()
{
    const auto code = SystemError::getLastOSErrorCode();
    ASSERT_TRUE(code == SystemError::wouldBlock || code == SystemError::again)
        << SystemError::toString(code).toStdString();
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleSyncFlags(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none,
    const QByteArray& testMessage = kTestMessage)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress))<< lastError();
    ASSERT_TRUE(server->listen(testClientCount())) << lastError();
    if (!endpointToConnectTo)
        endpointToConnectTo = server->getLocalAddress();

    Buffer buffer(kTestMessage.size() * 2, Qt::Uninitialized);
    const auto clientCount = testClientCount();
    for (size_t i = 0; i != clientCount; ++i)
    {
        std::unique_ptr<AbstractStreamSocket> accepted;
        nx::utils::thread acceptThread(
            [&]()
            {
                accepted.reset(server->accept());
                ASSERT_TRUE(accepted.get());
                EXPECT_EQ(readNBytes(accepted.get(), testMessage.size()), kTestMessage);
            });

        auto client = clientMaker();
        ASSERT_TRUE(client->connect(*endpointToConnectTo, kTestTimeout.count()));
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size())
            << lastError();
        acceptThread.join();

        // MSG_DONTWAIT does not block on server and client:
        ASSERT_EQ(accepted->recv(buffer.data(), buffer.size(), MSG_DONTWAIT), -1);
        testWouldBlockLastError();
        ASSERT_EQ(client->recv(buffer.data(), buffer.size(), MSG_DONTWAIT), -1);
        testWouldBlockLastError();

// TODO: Should be enabled and fixed for UDT and SSL sockets
#ifdef NX_TEST_MSG_WAITALL
        // MSG_WAITALL blocks until buffer is full.
        const auto recvWaitAll =
            [&](AbstractStreamSocket* socket)
            {
                int ret = socket->recv(buffer.data(), buffer.size(), MSG_WAITALL);
                if (ret == buffer.size())
                    EXPECT_EQ(buffer, (kTestMessage + kTestMessage).left(buffer.size()));
                else
                    EXPECT_EQ(ret, buffer.size()) << lastError();
            };

        // Send 1st part of message and start ot recv:
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size());
        nx::utils::thread serverRecvThread([&](){ recvWaitAll(accepted.get()); });

        // Send 2nd part of message with delay:
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size());
        serverRecvThread.join();

        // MSG_WAITALL works an client as well:
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size());
        nx::utils::thread clientRecvThread([&](){ recvWaitAll(accepted.get()); });
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size());
        clientRecvThread.join();
#endif
    };
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleAsync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo,
    const Buffer& testMessage = kTestMessage)
{
    nx::utils::TestSyncQueue< SystemError::ErrorCode > serverResults;
    nx::utils::TestSyncQueue< SystemError::ErrorCode > clientResults;

    const auto server = serverMaker();
    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedClients;
    const auto serverGuard = makeScopedGuard(
        [&]()
        {
            server->pleaseStopSync();
            for (const auto& socket : acceptedClients)
                socket->pleaseStopSync();
        });

    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(kTestTimeout.count() * 2));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << lastError();
    ASSERT_TRUE(server->listen(testClientCount())) << lastError();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logINFO);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    QByteArray serverBuffer;
    serverBuffer.reserve(128);
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> acceptor
        = [&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
        testDebugOutput(lm("Server accept: %1").arg(code));
        EXPECT_EQ(SystemError::noError, code);
        if (code != SystemError::noError)
            return serverResults.push(code);

        acceptedClients.emplace_back(socket);
        auto& client = acceptedClients.back();
        if (!client->setSendTimeout(kTestTimeout) ||
            !client->setSendTimeout(kTestTimeout) ||
            !client->setNonBlockingMode(true))
        {
            EXPECT_TRUE(false) << lastError();
            return serverResults.push(SystemError::notImplemented);
        }

        client->readAsyncAtLeast(
            &serverBuffer, testMessage.size(),
            [&](SystemError::ErrorCode code, size_t size)
            {
                testDebugOutput(lm("Server read: %1").arg(size));
                EXPECT_EQ(SystemError::noError, code);
                if (code != SystemError::noError)
                    return serverResults.push(code);

                EXPECT_STREQ(serverBuffer.data(), testMessage.data());
                if (size < testMessage.size())
                    return serverResults.push(SystemError::connectionReset);

                serverBuffer.resize(0);
                client->sendAsync(
                    testMessage,
                    [&](SystemError::ErrorCode code, size_t size)
                    {
                        testDebugOutput(lm("Server sent: %1").arg(size));
                        if (code == SystemError::noError)
                            EXPECT_GT(size, (size_t)0);

                        client->pleaseStopSync();
                        client->close();
                        testDebugOutput(lm("Server stopped"));
                        serverResults.push(code);
                    });
            });

        server->acceptAsync(acceptor);
    };

    server->acceptAsync(acceptor);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto clientCount = testClientCount();
    for (size_t i = 0; i != clientCount; ++i)
    {
        const auto testClient = clientMaker();
        const auto clientGuard = makeScopedGuard([&](){ testClient->pleaseStopSync(); });
        ASSERT_TRUE(testClient->setNonBlockingMode(true));
        ASSERT_TRUE(testClient->setSendTimeout(kTestTimeout.count()));
        ASSERT_TRUE(testClient->setRecvTimeout(kTestTimeout.count()));

        QByteArray clientBuffer;
        clientBuffer.reserve(128);
        testDebugOutput(lm("Client connecting"));
        testClient->connectAsync(*endpointToConnectTo, [&](SystemError::ErrorCode code)
        {
            testDebugOutput(lm("Client connected: %1").arg(code));
            ASSERT_EQ(code, SystemError::noError);
            testClient->sendAsync(
                testMessage,
                [&](SystemError::ErrorCode code, size_t size)
                {
                    testDebugOutput(lm("Client sent: %1").arg(size));
                    EXPECT_EQ(SystemError::noError, code);
                    if (code != SystemError::noError)
                        return clientResults.push(code);

                    EXPECT_EQ(code, SystemError::noError);
                    EXPECT_EQ(size, (size_t)testMessage.size());

                    auto buffer = std::make_shared<Buffer>();
                    buffer->reserve(128);
                    testClient->readAsyncAtLeast(
                        buffer.get(), testMessage.size(),
                        [&, buffer](SystemError::ErrorCode code, size_t size)
                    {
                        testDebugOutput(lm("Client read(1): %1").arg(size));
                        EXPECT_EQ(SystemError::noError, code);
                        if (code != SystemError::noError)
                            return clientResults.push(code);

                        EXPECT_GT(size, (size_t)0);
                        EXPECT_STREQ(buffer->data(), testMessage.data());

                        buffer->resize(0);
                        testClient->readAsyncAtLeast(
                            buffer.get(), testMessage.size(),
                            [&, buffer](SystemError::ErrorCode code, size_t size)
                            {
                                testDebugOutput(lm("Client read(2): %1").arg(size));
                                EXPECT_EQ(SystemError::noError, code);
                                EXPECT_EQ(size, (size_t)0);
                                EXPECT_EQ(buffer->size(), 0);
                                clientResults.push(code);
                            });
                    });
                });
        });

        ASSERT_EQ(clientResults.pop(), SystemError::noError) << i;
        ASSERT_EQ(serverResults.pop(), SystemError::noError) << i;
    }
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketMultiConnect(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    static const std::chrono::milliseconds timeout(1500);

    nx::utils::TestSyncQueue< SystemError::ErrorCode > acceptResults;
    nx::utils::TestSyncQueue< SystemError::ErrorCode > connectResults;

    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedSockets;
    std::vector<std::unique_ptr<AbstractStreamSocket>> connectedSockets;

    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(timeout.count()));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << lastError();
    ASSERT_TRUE(server->listen(testClientCount())) << lastError();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> acceptor = 
        [&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
        {
            acceptResults.push(code);
            if (code != SystemError::noError)
                return;

            acceptedSockets.emplace_back(socket);
            server->acceptAsync(acceptor);
        };

    server->acceptAsync(acceptor);
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // for UDT only

    std::function<void(int)> connectNewClients =
        [&](int clientsToConnect)
        {
            if (clientsToConnect == 0)
                return;

            auto testClient = clientMaker();
            ASSERT_TRUE(testClient->setNonBlockingMode(true));
            ASSERT_TRUE(testClient->setSendTimeout(timeout.count()));

            connectedSockets.push_back(std::move(testClient));
            connectedSockets.back()->connectAsync(
                *endpointToConnectTo,
                [&, clientsToConnect, connectNewClients]
                    (SystemError::ErrorCode code)
                {
                    connectResults.push(code);
                    if (code == SystemError::noError)
                        connectNewClients(clientsToConnect - 1);
                });
        };

    const auto kClientCount = testClientCount();
    connectNewClients(kClientCount);
    for (size_t i = 0; i < kClientCount; ++i)
    {
        ASSERT_EQ(acceptResults.pop(), SystemError::noError);
        ASSERT_EQ(connectResults.pop(), SystemError::noError);
    }

    server->pleaseStopSync();
    for (auto& socket : connectedSockets)
        socket->pleaseStopSync();
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketErrorHandling(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker)
{
    auto client = clientMaker();
    auto server = serverMaker();

    SystemError::setLastErrorCode(SystemError::noError);
    ASSERT_TRUE(client->bind(SocketAddress::anyAddress))
        << SystemError::getLastOSErrorText().toStdString();
    ASSERT_EQ(SystemError::getLastOSErrorCode(), SystemError::noError);

    SystemError::setLastErrorCode(SystemError::noError);
    ASSERT_FALSE(server->bind(client->getLocalAddress()));
    ASSERT_EQ(SystemError::getLastOSErrorCode(), SystemError::addrInUse);

    // Sounds wierd but linux ::listen sometimes returns true...
    //
    // SystemError::setLastErrorCode(SystemError::noError);
    // ASSERT_FALSE(server->listen(10));
    // ASSERT_NE(SystemError::getLastOSErrorCode(), SystemError::noError);
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketShutdown(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    bool useAsyncPriorSync,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    SocketAddress endpointToBindTo = SocketAddress::anyPrivateAddress;

    // Takes amazingly long with UdtSocket.
    const auto repeatCount = useAsyncPriorSync ? 5 : 14;
    for (int i = 0; i < repeatCount; ++i)
    {
        const auto syncServer = syncSocketServer(serverMaker());
        syncServer->setEndpointToBindTo(endpointToBindTo);
        syncServer->setErrorHandling(ErrorHandling::ignore);
        if (!useAsyncPriorSync)
            syncServer->setTestMessage(Buffer());

        auto serverAddress = syncServer->start();
        if (!endpointToConnectTo)
            endpointToConnectTo = std::move(serverAddress);

        // TODO: #mux Figure out why it fails on UdtSocket when address changes
        if (endpointToBindTo == SocketAddress::anyPrivateAddress)
            endpointToBindTo = std::move(serverAddress);

        auto client = clientMaker();
        ASSERT_TRUE(client->setSendTimeout(2 * kTestTimeout.count()));
        ASSERT_TRUE(client->setRecvTimeout(2 * kTestTimeout.count()));

        nx::utils::promise<void> testReadyPromise;
        nx::utils::promise<void> recvExitedPromise;
        nx::utils::thread clientThread(
            [&]()
            {
                if (useAsyncPriorSync)
                {
                    nx::utils::promise<void> asyncDone;
                    ASSERT_TRUE(client->setNonBlockingMode(true));
                    client->connectAsync(
                        *endpointToConnectTo,
                        [&](SystemError::ErrorCode code)
                        {
                            ASSERT_EQ(SystemError::noError, code);
                            client->sendAsync(
                                kTestMessage,
                                [&](SystemError::ErrorCode code, size_t /*size*/)
                                {
                                    ASSERT_EQ(SystemError::noError, code);
                                    client->post(
                                        [&]() { asyncDone.set_value(); });
                                });
                        });

                    asyncDone.get_future().wait();
                    ASSERT_TRUE(client->setNonBlockingMode(false));
                    testReadyPromise.set_value();
                }
                else
                {
                    client->connect(*endpointToConnectTo, kTestTimeout.count());
                }

                nx::Buffer readBuffer;
                readBuffer.resize(4096);
                for (;;)
                {
                    const int bytesRead = client->recv(readBuffer.data(), readBuffer.size(), 0);
                    if (bytesRead > 0)
                        continue;
                    if (bytesRead < 0 &&
                        SystemError::getLastOSErrorCode() == SystemError::wouldBlock)
                    {
                        continue;
                    }
                    break;  //connection closed
                }

                recvExitedPromise.set_value();
            });

        if (useAsyncPriorSync)
            testReadyPromise.get_future().wait();

        // Giving client thread some time to call client->recv.
        std::this_thread::sleep_for(std::chrono::milliseconds(nx::utils::random::number(0, 500)));

        // Testing that shutdown interrupts recv call.
        client->shutdown();

        ASSERT_EQ(
            std::future_status::ready,
            recvExitedPromise.get_future().wait_for(std::chrono::seconds(1)));

        clientThread.join();
    }
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleAcceptMixed(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    auto server = serverMaker();
    auto serverGuard = makeScopedGuard([&](){ server->pleaseStopSync(); });
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen(testClientCount()));

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    // no clients yet
    {
        std::unique_ptr<AbstractStreamSocket> accepted(server->accept());
        ASSERT_EQ(accepted, nullptr);
        ASSERT_EQ(SystemError::getLastOSErrorCode(), SystemError::wouldBlock);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto client = clientMaker();
    auto clientGuard = makeScopedGuard([&](){ client->pleaseStopSync(); });
    ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));
    ASSERT_TRUE(client->setNonBlockingMode(true));
    nx::utils::promise<SystemError::ErrorCode> connectionEstablishedPromise;
    client->connectAsync(
        *endpointToConnectTo,
        [&connectionEstablishedPromise](SystemError::ErrorCode code)
        {
            connectionEstablishedPromise.set_value(code);
        });

    auto connectionEstablishedFuture = connectionEstablishedPromise.get_future();
    // let the client get in the server listen queue
    ASSERT_EQ(
        std::future_status::ready,
        connectionEstablishedFuture.wait_for(std::chrono::seconds(1)));
    ASSERT_EQ(SystemError::noError, connectionEstablishedFuture.get());

    //if connect returned, it does not mean that accept has actually returned,
        //so giving internal socket implementation some time...
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::unique_ptr<AbstractStreamSocket> accepted(server->accept());
    ASSERT_NE(accepted, nullptr) << lastError();
}

template<typename ClientSocketMaker>
void socketSingleAioThread(const ClientSocketMaker& clientMaker)
{
    aio::AbstractAioThread* aioThread(nullptr);
    std::vector<decltype(clientMaker())> sockets;
    nx::utils::TestSyncQueue<nx::utils::thread::id> threadIdQueue;

    const auto clientCount = testClientCount();
    for (size_t i = 0; i < clientCount; ++i)
    {
        auto client = clientMaker();
        ASSERT_TRUE(client->setNonBlockingMode(true));
        ASSERT_TRUE(client->setSendTimeout(100));
        ASSERT_TRUE(client->setRecvTimeout(100));

        if (aioThread)
            client->bindToAioThread(aioThread);
        else
            aioThread = client->getAioThread();

        client->connectAsync("12.34.56.78:9999",
                             [&](SystemError::ErrorCode code)
        {
            EXPECT_NE(code, SystemError::noError);
            threadIdQueue.push(std::this_thread::get_id());
        });

        sockets.push_back(std::move(client));
    }

    boost::optional<nx::utils::thread::id> aioThreadId;
    for (size_t i = 0; i < clientCount; ++i)
    {
        const auto threadId = threadIdQueue.pop();
        if (aioThreadId)
            ASSERT_EQ(*aioThreadId, threadId);
        else
            aioThreadId = threadId;
    }

    for (auto& each : sockets)
        each->pleaseStopSync();
}

template<typename ClientSocketMaker>
void socketConnectToBadAddress(
    const ClientSocketMaker& clientMaker,
    bool deleteInIoThread)
{
    const std::chrono::seconds sendTimeout(1);

    auto client = clientMaker();
    ASSERT_TRUE(client->setNonBlockingMode(true));
    ASSERT_TRUE(client->setSendTimeout(
        std::chrono::duration_cast<std::chrono::milliseconds>(sendTimeout).count()));

    nx::utils::promise<SystemError::ErrorCode> connectCompletedPromise;
    client->connectAsync(
        SocketAddress("abdasdf:123"),
        [&](SystemError::ErrorCode errorCode)
        {
            if (deleteInIoThread)
                client.reset();

            connectCompletedPromise.set_value(errorCode);
        });

    const auto errorCode = connectCompletedPromise.get_future().get();
    ASSERT_NE(SystemError::noError, errorCode);
}

enum class StopType { cancelIo, pleaseStop };

template<typename ClientSocketMaker>
void socketConnectCancelSync(
    const ClientSocketMaker& clientMaker, StopType stopType)
{
    const auto clientCount = testClientCount();
    for (size_t i = 0; i < clientCount; ++i)
    {
        auto client = clientMaker();
        ASSERT_TRUE(client->setNonBlockingMode(true));
        client->connectAsync(SocketAddress("ya.ru:80"), [](SystemError::ErrorCode) { /*pass*/ });
        switch (stopType)
        {
            case StopType::cancelIo:
                client->cancelIOSync(aio::EventType::etWrite);
                break;
            case StopType::pleaseStop:
                client->pleaseStopSync();
                break;
            default:
                NX_CRITICAL(false);
        };
    }
}

template<typename ClientSocketMaker>
void socketConnectCancelAsync(
    const ClientSocketMaker& clientMaker, StopType stopType)
{
    std::vector<std::unique_ptr<AbstractStreamSocket>> clients;
    nx::utils::BarrierWaiter barrier;
    const auto clientCount = testClientCount();
    for (size_t i = 0; i < clientCount; ++i)
    {
        auto client = clientMaker();
        ASSERT_TRUE(client->setNonBlockingMode(true));
        client->connectAsync(SocketAddress("ya.ru:80"), [](SystemError::ErrorCode) { /*pass*/ });
        switch (stopType)
        {
            case StopType::cancelIo:
                client->cancelIOAsync(aio::EventType::etWrite, barrier.fork());
                break;
            case StopType::pleaseStop:
                client->pleaseStop(barrier.fork());
                break;
            default:
                NX_CRITICAL(false);
        };

        clients.push_back(std::move(client));
    }
}

template<typename ClientSocketMaker>
void socketIsInValidStateAfterCancellation(const ClientSocketMaker& clientMaker)
{
    auto socket = clientMaker();
    ASSERT_TRUE(socket->setNonBlockingMode(true));
    socket->connectAsync(
        SocketAddress(HostAddress::localhost, 12345),
        [](SystemError::ErrorCode /*sysErrorCode*/) {});

    socket->pleaseStopSync();
    socket->setRecvBufferSize(128 * 1024);
}

template<typename ServerSocketMaker>
void socketAcceptTimeoutSync(
    const ServerSocketMaker& serverMaker,
    std::chrono::milliseconds timeout = kTestTimeout)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(timeout.count()));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen(5));

    const auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(server->accept(), nullptr);
    EXPECT_EQ(SystemError::getLastOSErrorCode(), SystemError::timedOut);
    EXPECT_LT(std::chrono::steady_clock::now() - start, timeout * 2);
}

template<typename ServerSocketMaker>
void socketAcceptTimeoutAsync(
    const ServerSocketMaker& serverMaker,
    bool deleteInIoThread,
    std::chrono::milliseconds timeout = kTestTimeout)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(timeout.count()));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen(5));

    nx::utils::TestSyncQueue< SystemError::ErrorCode > serverResults;
    const auto start = std::chrono::steady_clock::now();
    server->acceptAsync([&](SystemError::ErrorCode /*code*/,
                            AbstractStreamSocket* /*socket*/)
    {
        if (deleteInIoThread)
            server.reset();

        serverResults.push(SystemError::timedOut);
    });

    EXPECT_EQ(serverResults.pop(), SystemError::timedOut);
    EXPECT_TRUE(std::chrono::steady_clock::now() - start < timeout * 2);
    if (server)
        server->pleaseStopSync();
}

template<typename ServerSocketMaker>
void socketAcceptCancelSync(
    const ServerSocketMaker& serverMaker, StopType stopType)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setRecvTimeout(kTestTimeout.count()));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen(5));

    const auto serverCount = testClientCount();
    for (size_t i = 0; i < serverCount; ++i)
    {
        server->acceptAsync([&](SystemError::ErrorCode, AbstractStreamSocket*) { /*pass*/ });
        if (i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));

        switch (stopType)
        {
            case StopType::cancelIo:
                server->cancelIOSync();
                break;
            case StopType::pleaseStop:
                server->pleaseStopSync();
                return; // is not recoverable
            default:
                NX_CRITICAL(false);
        };
    }
}

template<typename ServerSocketMaker>
void socketAcceptCancelAsync(
    const ServerSocketMaker& serverMaker, StopType stopType)
{
    std::vector<std::unique_ptr<AbstractStreamServerSocket>> servers;
    nx::utils::BarrierWaiter barrier;
    const auto serverCount = testClientCount();
    for (size_t i = 0; i < serverCount; ++i)
    {
        auto server = serverMaker();
        ASSERT_TRUE(server->setNonBlockingMode(true))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(server->setRecvTimeout(kTestTimeout.count()))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(server->listen(5))
            << SystemError::getLastOSErrorText().toStdString();

        server->acceptAsync(
            [&](SystemError::ErrorCode, AbstractStreamSocket*) { NX_CRITICAL(false); });

        if (i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * i));

        switch (stopType)
        {
            case StopType::cancelIo:
                server->cancelIOAsync(barrier.fork());
                break;
            case StopType::pleaseStop:
                server->pleaseStop(barrier.fork());
                break;
            default:
                NX_CRITICAL(false);
        };

        servers.push_back(std::move(server));
    }
}

template<typename ServerSocketMaker>
void serverSocketPleaseStopCancelsPostedCall(
    const ServerSocketMaker& serverMaker)
{
    auto serverSocket = serverMaker();
    nx::utils::promise<void> socketStopped;
    serverSocket->post(
        [&serverSocket, &socketStopped]()
        {
            serverSocket->post([]() { ASSERT_TRUE(false); });
            serverSocket->pleaseStopSync();
            socketStopped.set_value();
        });
    socketStopped.get_future().wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

} // namespace test
} // namespace network
} // namespace nx

typedef nx::network::test::StopType StopType;

#define NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, SingleAioThread) \
        { nx::network::test::socketSingleAioThread(mkClient); } \
    Type(Name, Shutdown) \
        { nx::network::test::socketShutdown(mkServer, mkClient, false, endpointToConnectTo); } \
    Type(Name, ShutdownAfterAsync) \
        { nx::network::test::socketShutdown(mkServer, mkClient, true, endpointToConnectTo); } \
    Type(Name, ConnectToBadAddress) \
        { nx::network::test::socketConnectToBadAddress(mkClient, false); } \
    Type(Name, ConnectToBadAddressIoDelete) \
        { nx::network::test::socketConnectToBadAddress(mkClient, true); } \
    Type(Name, ConnectCancelIoSync) \
        { nx::network::test::socketConnectCancelSync(mkClient, StopType::cancelIo); } \
    Type(Name, ConnectPleaseStopSync) \
        { nx::network::test::socketConnectCancelSync(mkClient, StopType::pleaseStop); } \
    Type(Name, ConnectCancelIoAsync) \
        { nx::network::test::socketConnectCancelAsync(mkClient, StopType::cancelIo); } \
    Type(Name, ConnectPleaseStopAsync) \
        { nx::network::test::socketConnectCancelAsync(mkClient, StopType::pleaseStop); } \
    Type(Name, SocketIsInValidStateAfterCancellation) \
        { nx::network::test::socketIsInValidStateAfterCancellation(mkClient); } \

#define NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, SimpleAcceptMixed) \
        { nx::network::test::socketSimpleAcceptMixed(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, AcceptTimeoutSync) \
        { nx::network::test::socketAcceptTimeoutSync(mkServer); } \
    Type(Name, AcceptTimeoutAsync) \
        { nx::network::test::socketAcceptTimeoutAsync(mkServer, false); } \
    Type(Name, AcceptTimeoutAsyncIoDelete) \
        { nx::network::test::socketAcceptTimeoutAsync(mkServer, true); } \
    Type(Name, AcceptCancelIoSync) \
        { nx::network::test::socketAcceptCancelSync(mkServer, StopType::cancelIo); } \
    Type(Name, AcceptPleaseStopSync) \
        { nx::network::test::socketAcceptCancelSync(mkServer, StopType::pleaseStop); } \
    Type(Name, AcceptCancelIoAsync) \
        { nx::network::test::socketAcceptCancelAsync(mkServer, StopType::cancelIo); } \
    Type(Name, AcceptPleaseStopAsync) \
        { nx::network::test::socketAcceptCancelAsync(mkServer, StopType::pleaseStop); } \
    Type(Name, PleaseStopCancelsPostedCall) \
        { nx::network::test::serverSocketPleaseStopCancelsPostedCall(mkServer); } \

#define NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, SimpleSync) \
        { nx::network::test::socketSimpleSync(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, SimpleSyncFlags) \
        { nx::network::test::socketSimpleSyncFlags(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, SimpleAsync) \
        { nx::network::test::socketSimpleAsync(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, SimpleMultiConnect) \
        { nx::network::test::socketMultiConnect(mkServer, mkClient, endpointToConnectTo); } \

#define NX_NETWORK_TRANSMIT_SOCKET_TESTS_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, boost::none)

#define NX_NETWORK_TRANSMIT_SOCKET_TESTS_CASE_EX(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo)

#define NX_NETWORK_CLIENT_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, boost::none) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, boost::none) \

#define NX_NETWORK_CLIENT_SOCKET_TEST_CASE_EX(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \

#define NX_NETWORK_SERVER_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, boost::none) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, boost::none) \

#define NX_NETWORK_BOTH_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, boost::none) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, boost::none) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, boost::none) \

