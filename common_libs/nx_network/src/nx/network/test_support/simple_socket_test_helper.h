#pragma once

#include <iostream>

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/network/async_stoppable.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace network {
namespace test {

namespace {

const QByteArray kTestMessage("Ping");
const std::chrono::milliseconds kTestTimeout(5000);

inline size_t testClientCount()
{
     return nx::utils::TestOptions::applyLoadMode<size_t>(10);
}

static const bool kEnableTestDebugOutput = false;
template<typename Message>
static inline void testDebugOutput(const Message& message)
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
        auto startedPromiseGuard = makeScopeGuard(
            [this]() { m_startedPromise.set_value(ServerStartResult(false, SocketAddress())); });

        ASSERT_TRUE(m_server->setReuseAddrFlag(true)) << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(m_server->bind(m_endpointToBindTo)) << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(m_server->listen((int)testClientCount())) << SystemError::getLastOSErrorText().toStdString();

        ASSERT_TRUE(m_server->setRecvTimeout(100)) << SystemError::getLastOSErrorText().toStdString();
        std::unique_ptr<AbstractStreamSocket> client(m_server->accept());
        ASSERT_FALSE(client);
        ASSERT_EQ(SystemError::timedOut, SystemError::getLastOSErrorCode());

        auto serverAddress = m_server->getLocalAddress();

        NX_LOGX(lm("Started on %1").arg(serverAddress.toString()), cl_logINFO);
        startedPromiseGuard.disarm();
        m_startedPromise.set_value(ServerStartResult(true, std::move(serverAddress)));

        const auto startTime = std::chrono::steady_clock::now();
        const auto maxTimeout = kTestTimeout * testClientCount();
        int acceptedConnectionCount = 0;
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

            ++acceptedConnectionCount;

            ASSERT_TRUE(client.get()) << SystemError::getLastOSErrorText().toStdString();
            ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
            ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

            if (m_testMessage.isEmpty())
                continue;

            const auto incomingMessage = readNBytes(client.get(), m_testMessage.size());
            if (m_errorHandling == ErrorHandling::triggerAssert)
            {
                ASSERT_TRUE(!incomingMessage.isEmpty()) << SystemError::getLastOSErrorText().toStdString();
                ASSERT_EQ(m_testMessage, incomingMessage);
            }

            const int bytesSent = client->send(m_testMessage);
            if (m_errorHandling == ErrorHandling::triggerAssert)
            {
                ASSERT_NE(-1, bytesSent) << SystemError::getLastOSErrorText().toStdString();
            }

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
void socketTransferSync(
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
                    << i << ": " << SystemError::getLastOSErrorText().toStdString();

                ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
                ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

                EXPECT_EQ(
                    testMessage.size(),
                    client->send(testMessage.constData(), testMessage.size())) << SystemError::getLastOSErrorText().toStdString();

                const auto incomingMessage = readNBytes(
                    client.get(), testMessage.size());

                ASSERT_TRUE(!incomingMessage.isEmpty()) << i << ": " << SystemError::getLastOSErrorText().toStdString();
                ASSERT_EQ(testMessage, incomingMessage);
                client.reset();
            }
        });

    clientThread.join();
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketTransferSyncFlags(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none,
    const QByteArray& testMessage = kTestMessage)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress))<< SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen((int)testClientCount())) << SystemError::getLastOSErrorText().toStdString();
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
        auto acceptThreadGuard = makeScopeGuard([&acceptThread]() { acceptThread.join(); });

        auto client = clientMaker();
        ASSERT_TRUE(client->connect(*endpointToConnectTo, kTestTimeout.count()));
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size())
            << SystemError::getLastOSErrorText().toStdString();
        acceptThreadGuard.fire();

        // MSG_DONTWAIT does not block on server and client:
        ASSERT_EQ(accepted->recv(buffer.data(), buffer.size(), MSG_DONTWAIT), -1);
        auto sysErrorCode = SystemError::getLastOSErrorCode();
        ASSERT_TRUE(sysErrorCode == SystemError::wouldBlock || sysErrorCode == SystemError::again)
            << SystemError::toString(sysErrorCode).toStdString();

        ASSERT_EQ(client->recv(buffer.data(), buffer.size(), MSG_DONTWAIT), -1);
        sysErrorCode = SystemError::getLastOSErrorCode();
        ASSERT_TRUE(sysErrorCode == SystemError::wouldBlock || sysErrorCode == SystemError::again)
            << SystemError::toString(sysErrorCode).toStdString();

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
                    EXPECT_EQ(ret, buffer.size()) << SystemError::getLastOSErrorText().toStdString();
            };

        // Send 1st part of message and start ot recv:
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size());
        nx::utils::thread serverRecvThread([&](){ recvWaitAll(accepted.get()); });
        auto serverRecvThreadGuard =
            makeScopeGuard([&serverRecvThread]() { serverRecvThread.join(); });

        // Send 2nd part of message with delay:
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size());
        serverRecvThreadGuard.fire();

        // MSG_WAITALL works an client as well:
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size());
        nx::utils::thread clientRecvThread([&](){ recvWaitAll(accepted.get()); });
        auto clientRecvThreadGuard =
            makeScopeGuard([&clientRecvThread]() { clientRecvThread.join(); });

        std::this_thread::sleep_for(std::chrono::microseconds(500));
        ASSERT_EQ(client->send(testMessage.data(), testMessage.size()), testMessage.size());
        clientRecvThreadGuard.fire();
#endif
    };
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketTransferAsync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo,
    const Buffer& testMessage = kTestMessage)
{
    nx::utils::TestSyncQueue< SystemError::ErrorCode > serverResults;
    nx::utils::TestSyncQueue< SystemError::ErrorCode > clientResults;

    const auto server = serverMaker();
    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedClients;
    const auto serverGuard = makeScopeGuard(
        [&]()
        {
            server->pleaseStopSync();
            for (const auto& socket : acceptedClients)
                socket->pleaseStopSync();
        });

    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    //ASSERT_TRUE(server->setRecvTimeout(kTestTimeout.count() * 2));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen((int)testClientCount())) << SystemError::getLastOSErrorText().toStdString();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logINFO);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    QByteArray serverBuffer;
    serverBuffer.reserve(128);
    std::function<void(SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>)> acceptor =
        [&](SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
        {
            testDebugOutput(lm("Server accept: %1").arg(code));
            EXPECT_EQ(SystemError::noError, code);
            if (code != SystemError::noError)
                return serverResults.push(code);

            acceptedClients.emplace_back(std::move(socket));
            auto& client = acceptedClients.back();
            if (/*!client->setSendTimeout(kTestTimeout) ||
                !client->setSendTimeout(kTestTimeout) ||*/
                !client->setNonBlockingMode(true))
            {
                EXPECT_TRUE(false) << SystemError::getLastOSErrorText().toStdString();
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
                    if (size < (size_t)testMessage.size())
                        return serverResults.push(SystemError::connectionReset);

                    serverBuffer.resize(0);
                    client->sendAsync(
                        testMessage,
                        [&](SystemError::ErrorCode code, size_t size)
                        {
                            testDebugOutput(lm("Server sent: %1").arg(size));
                            if (code == SystemError::noError)
                            {
                                EXPECT_GT(size, (size_t)0);
                            }

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
        const auto clientGuard = makeScopeGuard([&](){ testClient->pleaseStopSync(); });
        ASSERT_TRUE(testClient->setNonBlockingMode(true));
        //ASSERT_TRUE(testClient->setSendTimeout(kTestTimeout.count()));
        //ASSERT_TRUE(testClient->setRecvTimeout(kTestTimeout.count()));

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

template<typename Sender, typename Receiver>
inline void transferSyncAsync(Sender* sender, Receiver* receiver)
{
    ASSERT_EQ(sender->AbstractCommunicatingSocket::send(kTestMessage), kTestMessage.size()) << SystemError::getLastOSErrorText().toStdString();

    Buffer buffer;
    buffer.reserve(kTestMessage.size());

    nx::utils::promise<void> promise;
    receiver->readAsyncAtLeast(
        &buffer, kTestMessage.size(),
        [&](SystemError::ErrorCode code, size_t size)
        {
            EXPECT_EQ(SystemError::noError, code) << SystemError::toString(code).toStdString();
            EXPECT_EQ(size, (size_t)kTestMessage.size());
            EXPECT_EQ(buffer, kTestMessage);
            promise.set_value();
        });

    promise.get_future().wait();
}

template<typename Sender, typename Receiver>
static inline void transferAsyncSync(Sender* sender, Receiver* receiver)
{
    nx::utils::promise<void> promise;
    sender->sendAsync(
        kTestMessage,
        [&](SystemError::ErrorCode code, size_t size)
        {
            EXPECT_EQ(SystemError::noError, code) << SystemError::toString(code).toStdString();
            EXPECT_EQ((size_t)kTestMessage.size(), size);
            promise.set_value();
        });

    Buffer buffer(kTestMessage.size(), Qt::Uninitialized);
    EXPECT_EQ(buffer.size(), receiver->recv(buffer.data(), buffer.size(), MSG_WAITALL)) << SystemError::getLastOSErrorText().toStdString();
    EXPECT_EQ(buffer, kTestMessage);
    promise.get_future().wait();
}

template<typename Sender, typename Receiver>
static inline void transferSync(Sender* sender, Receiver* receiver)
{
    ASSERT_EQ(sender->AbstractCommunicatingSocket::send(kTestMessage), kTestMessage.size()) << SystemError::getLastOSErrorText().toStdString();

    Buffer buffer(kTestMessage.size(), Qt::Uninitialized);
    EXPECT_EQ(buffer.size(), receiver->recv(buffer.data(), buffer.size(), MSG_WAITALL)) << SystemError::getLastOSErrorText().toStdString();
}

template<typename Sender, typename Receiver>
static inline void transferAsync(Sender* sender, Receiver* receiver)
{
    nx::utils::promise<void> sendPromise;
    sender->sendAsync(
        kTestMessage,
        [&](SystemError::ErrorCode code, size_t size)
        {
            ASSERT_EQ(SystemError::noError, code) << SystemError::toString(code).toStdString();
            ASSERT_EQ((size_t)kTestMessage.size(), size);
            sendPromise.set_value();
        });

    Buffer buffer;
    buffer.reserve(kTestMessage.size());

    nx::utils::promise<void> readPromise;
    receiver->readAsyncAtLeast(
        &buffer, kTestMessage.size(),
        [&](SystemError::ErrorCode code, size_t size)
        {
            EXPECT_EQ(SystemError::noError, code) << SystemError::toString(code).toStdString();
            EXPECT_EQ((size_t)kTestMessage.size(), size);
            readPromise.set_value();
        });

    sendPromise.get_future().wait();
    readPromise.get_future().wait();
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSyncAsyncSwitch(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    bool bothNonBlocking,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(100));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen((int)testClientCount())) << SystemError::getLastOSErrorText().toStdString();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    ASSERT_FALSE((bool) server->accept()) << SystemError::getLastOSErrorText().toStdString();

    auto client = clientMaker();
    const auto clientGuard = makeScopeGuard([&client]() { client->pleaseStopSync(); });
    ASSERT_TRUE(client->setNonBlockingMode(true));
    ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));
    ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));

    nx::utils::promise<void> connectPromise;
    client->connectAsync(
        *endpointToConnectTo,
        [&](SystemError::ErrorCode code)
        {
            EXPECT_EQ(code, SystemError::noError);
            connectPromise.set_value();
        });

    connectPromise.get_future().wait();
    std::unique_ptr<AbstractStreamSocket> accepted(server->accept());
    ASSERT_TRUE((bool) accepted);
    const auto acceptedGuard = makeScopeGuard([&accepted]() { accepted->pleaseStopSync(); });

    ASSERT_TRUE(accepted->setSendTimeout(kTestTimeout.count()));
    ASSERT_TRUE(accepted->setRecvTimeout(kTestTimeout.count()));
    transferAsyncSync(client.get(), accepted.get());
    transferSyncAsync(accepted.get(), client.get());

    if (bothNonBlocking)
    {
        ASSERT_TRUE(accepted->setNonBlockingMode(true));
        transferAsync(client.get(), accepted.get());
        transferAsync(accepted.get(), client.get());
        ASSERT_TRUE(client->setNonBlockingMode(false));
    }
    else
    {
        ASSERT_TRUE(client->setNonBlockingMode(false));
        transferSync(client.get(), accepted.get());
        transferSync(accepted.get(), client.get());
        ASSERT_TRUE(accepted->setNonBlockingMode(true));
    }

    transferSyncAsync(client.get(), accepted.get());
    transferAsyncSync(accepted.get(), client.get());
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketTransferFragmentation(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    // On localhost TCP connection small packets usually transferred entirely,
    // so that we expect the same behavior for all our stream sockets.
    static const Buffer kMessage = utils::random::generate(100);
    static const size_t kTestRuns = utils::TestOptions::applyLoadMode<size_t>(5);

    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen((int)testClientCount())) << SystemError::getLastOSErrorText().toStdString();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    auto client = clientMaker();
    ASSERT_TRUE(client->connect(*endpointToConnectTo, kTestTimeout.count()));
    ASSERT_TRUE(client->setNonBlockingMode(true));
    const auto clientGuard = makeScopeGuard([&](){ client->pleaseStopSync(); });

    std::unique_ptr<AbstractStreamSocket> accepted(server->accept());
    ASSERT_NE(nullptr, accepted);

    for (size_t runNumber = 0; runNumber <= kTestRuns; ++runNumber)
    {
        NX_LOG(lm("Start transfer %1").arg(runNumber), cl_logDEBUG1);
        nx::utils::promise<void> promise;
        client->sendAsync(
            kMessage,
            [&](SystemError::ErrorCode code, size_t size)
            {
                ASSERT_EQ(SystemError::noError, code) << SystemError::toString(code).toStdString();
                ASSERT_EQ((size_t)kMessage.size(), size);
                promise.set_value();
            });

        Buffer buffer(kMessage.size(), Qt::Uninitialized);
        ASSERT_EQ(buffer.size(), accepted->recv(buffer.data(), buffer.size())) << SystemError::getLastOSErrorText().toStdString();
        ASSERT_EQ(buffer, kMessage);
        promise.get_future().wait();
    }
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketMultiConnect(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    QnMutex connectedSocketsMutex;
    bool terminated = false;
    nx::utils::TestSyncQueue< SystemError::ErrorCode > acceptResults;
    nx::utils::TestSyncQueue< SystemError::ErrorCode > connectResults;
    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedSockets;
    std::vector<std::unique_ptr<AbstractStreamSocket>> connectedSockets;
    decltype(serverMaker()) server;

    std::function<void(SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>)> acceptor =
        [&](SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
        {
            acceptResults.push(code);
            if (code != SystemError::noError)
                return;

            acceptedSockets.emplace_back(std::move(socket));
            server->acceptAsync(acceptor);
        };

    std::function<void(int)> connectNewClients =
        [&](int clientsToConnect)
        {
            QnMutexLocker lock(&connectedSocketsMutex);

            if (clientsToConnect == 0 || terminated)
                return;

            auto testClient = clientMaker();
            ASSERT_TRUE(testClient->setNonBlockingMode(true));
            connectedSockets.push_back(std::move(testClient));

            connectedSockets.back()->connectAsync(
                *endpointToConnectTo,
                [clientsToConnect, connectNewClients, &connectResults]
                    (SystemError::ErrorCode code)
                {
                    connectResults.push(code);
                    if (code == SystemError::noError)
                        connectNewClients(clientsToConnect - 1);
                });
        };

    server = serverMaker();
    auto serverGuard = makeScopeGuard([&server]() { server->pleaseStopSync(); });
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen((int)testClientCount())) << SystemError::getLastOSErrorText().toStdString();

    auto connectedSocketsGuard = makeScopeGuard(
        [&connectedSockets, &connectedSocketsMutex, &terminated]()
        {
            {
                QnMutexLocker lock(&connectedSocketsMutex);
                terminated = true;
            }

            for (auto& socket: connectedSockets)
                socket->pleaseStopSync();
        });

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    server->acceptAsync(acceptor);
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // for UDT only

    const auto kClientCount = testClientCount();
    connectNewClients((int)kClientCount);
    for (size_t i = 0; i < kClientCount; ++i)
    {
        ASSERT_EQ(acceptResults.pop(), SystemError::noError);
        ASSERT_EQ(connectResults.pop(), SystemError::noError);
    }
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketShutdown(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    bool useAsyncPriorSync,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    SocketAddress endpointToBindTo = SocketAddress::anyPrivateAddress;

    const auto repeatCount = utils::TestOptions::applyLoadMode<size_t>(5);
    for (size_t i = 0; i < repeatCount; ++i)
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
void socketAcceptMixed(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    auto server = serverMaker();
    auto serverGuard = makeScopeGuard([&](){ server->pleaseStopSync(); });
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen((int)testClientCount()));

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    // no clients yet
    {
        std::unique_ptr<AbstractStreamSocket> accepted(server->accept());
        ASSERT_EQ(nullptr, accepted);
        ASSERT_EQ(SystemError::wouldBlock, SystemError::getLastOSErrorCode());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto client = clientMaker();
    auto clientGuard = makeScopeGuard([&](){ client->pleaseStopSync(); });
    ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));
    ASSERT_TRUE(client->setNonBlockingMode(true));
    nx::utils::promise<SystemError::ErrorCode> connectionEstablishedPromise;
    client->connectAsync(
        *endpointToConnectTo,
        [&connectionEstablishedPromise](SystemError::ErrorCode code)
        {
            connectionEstablishedPromise.set_value(code);
        });

    ASSERT_EQ(SystemError::noError, connectionEstablishedPromise.get_future().get());

    //if connect returned, it does not mean that accept has actually returned,
        //so giving internal socket implementation some time...
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::unique_ptr<AbstractStreamSocket> accepted(server->accept());
    ASSERT_NE(accepted, nullptr) << SystemError::getLastOSErrorText().toStdString();
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void acceptedSocketOptionsInheritance(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker)
{
    auto server = serverMaker();
    auto serverGuard = makeScopeGuard([&](){ server->pleaseStopSync(); });
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(10 * 1000));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen((int)testClientCount()));
    auto serverAddress = server->getLocalAddress();

    auto client = clientMaker();
    ASSERT_TRUE(client->connect(serverAddress, 3000));

    std::unique_ptr<AbstractStreamSocket> accepted(server->accept());
    ASSERT_TRUE((bool)accepted);

    unsigned int acceptedSocketRecvTimeout = 0;
    ASSERT_TRUE(accepted->getRecvTimeout(&acceptedSocketRecvTimeout));
    unsigned int acceptedSocketSendTimeout = 0;
    ASSERT_TRUE(accepted->getSendTimeout(&acceptedSocketSendTimeout));

    // Timeouts are not inherited.
    ASSERT_EQ(0U, acceptedSocketRecvTimeout);
    ASSERT_EQ(0U, acceptedSocketSendTimeout);
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
void socketIsValidAfterPleaseStop(const ClientSocketMaker& clientMaker)
{
    auto socket = clientMaker();
    ASSERT_TRUE(socket->setNonBlockingMode(true));
    socket->connectAsync(
        SocketAddress(HostAddress::localhost, 12345),
        [](SystemError::ErrorCode /*sysErrorCode*/) {});

    socket->pleaseStopSync();
    socket->setRecvBufferSize(128 * 1024);
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketIsUsefulAfterCancelIo(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    static const std::chrono::milliseconds kMinDelay(1), kMaxDelay(1000);

    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(100));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen((int)testClientCount())) << SystemError::getLastOSErrorText().toStdString();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logINFO);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    ASSERT_FALSE((bool) server->accept());

    auto client = clientMaker();
    ASSERT_TRUE(client->setNonBlockingMode(true));
    ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));
    ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
    ASSERT_TRUE(client->connect(*endpointToConnectTo, kTestTimeout.count()));
    ASSERT_TRUE(client->setNonBlockingMode(true));

    ASSERT_TRUE(server->setRecvTimeout(0));
    std::unique_ptr<AbstractStreamSocket> accepted(server->accept());
    ASSERT_TRUE((bool) accepted);
    transferAsyncSync(client.get(), accepted.get());
    transferSyncAsync(accepted.get(), client.get());

    nx::Buffer buffer;
    buffer.reserve(100);
    for (std::chrono::milliseconds delay = kMinDelay; delay <= kMaxDelay; delay *= 10)
    {
        NX_LOG(lm("Cancel read: %1").arg(delay), cl_logINFO);
        client->readSomeAsync(&buffer, [](SystemError::ErrorCode, size_t) { FAIL(); });

        std::this_thread::sleep_for(delay);
        client->cancelIOSync(aio::EventType::etRead);

        transferSyncAsync(accepted.get(), client.get());
        transferAsyncSync(client.get(), accepted.get());
    }

    for (std::chrono::milliseconds delay = kMinDelay; delay <= kMaxDelay; delay *= 10)
    {
        NX_LOG(lm("Cancel write: %1").arg(delay), cl_logINFO);
        client->sendAsync(kTestMessage, [](SystemError::ErrorCode, size_t) { /*pass*/ });

        std::this_thread::sleep_for(delay);
        client->cancelIOSync(aio::EventType::etWrite);
    }

    buffer.resize(100);
    ASSERT_GT(accepted->recv(buffer.data(), buffer.size()), 0);
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
    server->acceptAsync(
        [&](
            SystemError::ErrorCode /*code*/,
            std::unique_ptr<AbstractStreamSocket> /*socket*/)
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
    auto serverGuard = makeScopeGuard([&server]() { server->pleaseStopSync(); });
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setRecvTimeout(kTestTimeout.count()));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen(5));

    const auto serverCount = testClientCount();
    for (size_t i = 0; i < serverCount; ++i)
    {
        server->acceptAsync(
            [&](SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>) { /*pass*/ });
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
            [&](SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>) { NX_CRITICAL(false); });

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

//-------------------------------------------------------------------------------------------------

template<typename CreateSocketFunc>
class SocketOptions:
    public ::testing::Test
{
public:
    using SocketPointer = typename std::result_of<CreateSocketFunc()>::type;

protected:
    std::vector<SocketPointer> m_sockets;

    void givenSocketBoundToLocalAddress()
    {
        addSocket();
        ASSERT_TRUE(m_sockets.back()->bind(SocketAddress::anyPrivateAddress));
    }

    void assertAnotherSocketCanBeBoundToTheSameAddress()
    {
        addSocket();
        ASSERT_TRUE(m_sockets.back()->bind(m_sockets.front()->getLocalAddress()));
    }

    void assertAnotherSocketCannotBeBoundToTheSameAddress()
    {
        addSocket();
        ASSERT_FALSE(m_sockets.back()->bind(m_sockets.front()->getLocalAddress()));
    }

    void addSocket()
    {
        m_sockets.push_back(CreateSocketFunc()());
        if (m_reuseAddr)
        {
            ASSERT_TRUE(m_sockets.back()->setReuseAddrFlag(true));
            ASSERT_TRUE(m_sockets.back()->setReusePortFlag(true));
        }
    }

    void enableReuseAddr()
    {
        m_reuseAddr = true;
    }

private:
    bool m_reuseAddr = false;
};

TYPED_TEST_CASE_P(SocketOptions);

TYPED_TEST_P(SocketOptions, same_address_can_be_used_if_reuse_addr_enabled)
{
    this->enableReuseAddr();

    this->givenSocketBoundToLocalAddress();
    this->assertAnotherSocketCanBeBoundToTheSameAddress();
}

TYPED_TEST_P(SocketOptions, same_address_cannot_be_used_by_default)
{
    this->givenSocketBoundToLocalAddress();
    this->assertAnotherSocketCannotBeBoundToTheSameAddress();
}

REGISTER_TYPED_TEST_CASE_P(SocketOptions,
    same_address_can_be_used_if_reuse_addr_enabled,
    same_address_cannot_be_used_by_default);

//-------------------------------------------------------------------------------------------------

template<typename CreateSocketFunc>
class SocketOptionsDefaultValue:
    public SocketOptions<CreateSocketFunc>
{
public:
    SocketOptionsDefaultValue()
    {
        this->addSocket();
    }

protected:
    template<typename SocketInterface, typename SocketOption>
    SocketOption getOptionValue(
        bool (SocketInterface::*getSocketOption)(SocketOption*) const)
    {
        auto value = SocketOption();
        NX_GTEST_ASSERT_TRUE((this->m_sockets.front().get()->*getSocketOption)(&value));
        return value;
    }
};

TYPED_TEST_CASE_P(SocketOptionsDefaultValue);

TYPED_TEST_P(SocketOptionsDefaultValue, reuse_addr)
{
    ASSERT_EQ(false, this->getOptionValue(&AbstractSocket::getReuseAddrFlag));
}

TYPED_TEST_P(SocketOptionsDefaultValue, non_blocking_mode)
{
    ASSERT_EQ(false, this->getOptionValue(&AbstractSocket::getNonBlockingMode));
}

REGISTER_TYPED_TEST_CASE_P(SocketOptionsDefaultValue,
    reuse_addr,
    non_blocking_mode);

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
    Type(Name, ValidAfterPleaseStop) \
        { nx::network::test::socketIsValidAfterPleaseStop(mkClient); } \
    Type(Name, UsefulAfterCancelIo) \
        { nx::network::test::socketIsUsefulAfterCancelIo(mkServer, mkClient, endpointToConnectTo); } \

#define NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, AcceptMixed) \
        { nx::network::test::socketAcceptMixed(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, AcceptedSocketOptionsInheritance) \
        { nx::network::test::acceptedSocketOptionsInheritance(mkServer, mkClient); } \
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

#define NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, TransferSync) \
        { nx::network::test::socketTransferSync(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, TransferSyncFlags) \
        { nx::network::test::socketTransferSyncFlags(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, TransferAsync) \
        { nx::network::test::socketTransferAsync(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, TransferSyncAsyncSwitch) \
        { nx::network::test::socketSyncAsyncSwitch(mkServer, mkClient, true, endpointToConnectTo); } \
    Type(Name, TransferAsyncSyncSwitch) \
        { nx::network::test::socketSyncAsyncSwitch(mkServer, mkClient, false, endpointToConnectTo); } \
    Type(Name, TransferFragmentation) \
        { nx::network::test::socketTransferFragmentation(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, MultiConnect) \
        { nx::network::test::socketMultiConnect(mkServer, mkClient, endpointToConnectTo); } \

#define NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, boost::none)

#define NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE_EX(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo)

#define NX_NETWORK_CLIENT_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, boost::none) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, boost::none) \

#define NX_NETWORK_CLIENT_SOCKET_TEST_CASE_EX(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \

#define NX_NETWORK_SERVER_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, boost::none) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, boost::none) \

#define NX_NETWORK_BOTH_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, boost::none) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, boost::none) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, boost::none) \

