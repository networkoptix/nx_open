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
#include <utils/common/guard.h>
#include <utils/common/stoppable.h>
#include <utils/common/systemerror.h>

//#define SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT

namespace nx {
namespace network {
namespace test {

namespace {

const QByteArray kTestMessage("Ping");
const int kClientCount(10);
const std::chrono::milliseconds kTestTimeout(5000);
std::string lastError() { return SystemError::getLastOSErrorText().toStdString(); }

} // namespace

enum class ActionOnReadWriteError
{
    ignore,
    triggerAssert
};

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

template<typename ServerSocketType>
void syncSocketServerMainFunc(
    const SocketAddress& endpointToBindTo,
    Buffer testMessage,
    int clientCount,
    ServerSocketType server,
    nx::utils::promise<SocketAddress>* startedPromise,
    ActionOnReadWriteError actionOnReadWriteError)
{
    ASSERT_TRUE(server->setReuseAddrFlag(true)) << lastError();
    ASSERT_TRUE(server->bind(endpointToBindTo)) << lastError();
    ASSERT_TRUE(server->listen(clientCount)) << lastError();
    if (startedPromise)
    {
        ASSERT_TRUE(server->setRecvTimeout(100)) << lastError();
        std::unique_ptr<AbstractStreamSocket> client(server->accept());
        ASSERT_FALSE(client);
        ASSERT_EQ(SystemError::timedOut, SystemError::getLastOSErrorCode());

        auto serverAddress = server->getLocalAddress();
        NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
        startedPromise->set_value(std::move(serverAddress));
    }

    ASSERT_TRUE(server->setRecvTimeout(kTestTimeout.count() * 10)) << lastError();
    for (int i = clientCount; i > 0; --i)
    {
        std::unique_ptr<AbstractStreamSocket> client(server->accept());
        if (actionOnReadWriteError == ActionOnReadWriteError::ignore && !client)
            continue;

        ASSERT_TRUE(client.get()) << lastError() << " on " << i;
        ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
        ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

        if (testMessage.isEmpty())
            continue;

        const auto incomingMessage = readNBytes(client.get(), testMessage.size());
        if (actionOnReadWriteError == ActionOnReadWriteError::triggerAssert)
        {
            ASSERT_TRUE(!incomingMessage.isEmpty()) << lastError();
            ASSERT_EQ(testMessage, incomingMessage);
        }

        const int bytesSent = client->send(testMessage);
        if (actionOnReadWriteError == ActionOnReadWriteError::triggerAssert)
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

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleSync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    nx::utils::TestOptions::applyLoadMode(clientCount);
    auto server = serverMaker();
    nx::utils::promise<SocketAddress> promise;
    nx::utils::thread serverThread(
        &syncSocketServerMainFunc<decltype(server)>,
        SocketAddress::anyPrivateAddress,
        testMessage,
        clientCount,
        std::move(server),
        &promise,
        ActionOnReadWriteError::triggerAssert);

    auto serverAddress = promise.get_future().get();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    nx::utils::thread clientThread(
        [endpointToConnectTo, &testMessage, clientCount, &clientMaker]()
        {
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

    serverThread.join();
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
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress))<< lastError();
    ASSERT_TRUE(server->listen(clientCount)) << lastError();
    if (!endpointToConnectTo)
        endpointToConnectTo = server->getLocalAddress();

    Buffer buffer(kTestMessage.size() * 2, Qt::Uninitialized);
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

template<typename ServerSocketMaker, typename ClientSocketMaker,
         typename StopSocketFunc>
void socketSimpleAsync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo,
    const QByteArray& testMessage,
    int clientCount,
    StopSocketFunc stopSocket)
{
    nx::utils::TestOptions::applyLoadMode(clientCount);
    nx::utils::TestSyncQueue< SystemError::ErrorCode > serverResults;
    nx::utils::TestSyncQueue< SystemError::ErrorCode > clientResults;

    auto server = serverMaker();

    const auto serverGuard = makeScopedGuard(
        [&server, &stopSocket]()
        {
            stopSocket(std::move(server));
        });

    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(kTestTimeout.count() * 2));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << lastError();
    ASSERT_TRUE(server->listen(clientCount)) << lastError();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
    static const auto t0 = std::chrono::steady_clock::now();
    static std::mutex mtx;
#endif

    QByteArray serverBuffer;
    serverBuffer.reserve(128);
    std::vector<std::unique_ptr<AbstractStreamSocket>> clients;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> acceptor
        = [&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
        {
            std::lock_guard<std::mutex> lk(mtx);
            std::cout
                <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                <<". accept "<<(int)code
                <<std::endl;
        }
#endif

        serverResults.push(code);
        if (code != SystemError::noError)
            return;

        clients.emplace_back(socket);
        auto& client = clients.back();
        ASSERT_TRUE(client->setSendTimeout(kTestTimeout));
        ASSERT_TRUE(client->setRecvTimeout(kTestTimeout));
        ASSERT_TRUE(client->setNonBlockingMode(true));
        client->readAsyncAtLeast(
            &serverBuffer, testMessage.size(),
            [&](SystemError::ErrorCode code, size_t size)
            {
#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
                {
                    std::lock_guard<std::mutex> lk(mtx);
                    std::cout
                        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                        << ". server-client. read " << (int)size
                        << std::endl;
                }
#endif

                if (code != SystemError::noError)
                {
                    stopSocket(std::move(client));
                    return serverResults.push(code);
                }

                EXPECT_GT(size, (size_t)0);
                EXPECT_STREQ(serverBuffer.data(), testMessage.data());
                serverBuffer.resize(0);

                client->sendAsync(
                    testMessage,
                    [&](SystemError::ErrorCode code, size_t size)
                    {
#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
                        {
                            std::lock_guard<std::mutex> lk(mtx);
                            std::cout
                                << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                                << ". server-client. sent " << (int)size
                                << std::endl;
                        }
#endif

                        if (code == SystemError::noError)
                            EXPECT_GT(size, (size_t)0);

                        stopSocket(std::move(client));

#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
                        {
                            std::lock_guard<std::mutex> lk(mtx);
                            std::cout
                                << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                                << ". server-client. stopped socket "
                                << std::endl;
                        }
#endif

                        serverResults.push(code);
                    });
            });

        server->acceptAsync(acceptor);
    };

    server->acceptAsync(acceptor);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (int i = clientCount; i > 0; --i)
    {
        auto testClient = clientMaker();

        const auto testClientGuard = makeScopedGuard(
            [&testClient, &stopSocket]()
            {
                stopSocket(std::move(testClient));
            });

        ASSERT_TRUE(testClient->setNonBlockingMode(true));
        ASSERT_TRUE(testClient->setSendTimeout(kTestTimeout.count()));
        ASSERT_TRUE(testClient->setRecvTimeout(kTestTimeout.count()));

        QByteArray clientBuffer;
        clientBuffer.reserve(128);
#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
        {
            std::lock_guard<std::mutex> lk(mtx);
            std::cout
                << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                << ". connecting "
                << std::endl;
        }
#endif
        testClient->connectAsync(*endpointToConnectTo, [&](SystemError::ErrorCode code)
        {
#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
            {
                std::lock_guard<std::mutex> lk(mtx);
                std::cout
                    << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                    << ". connected "<<(int)code
                    << std::endl;
            }
#endif

            ASSERT_EQ(code, SystemError::noError);
            testClient->sendAsync(
                testMessage,
                [&](SystemError::ErrorCode code, size_t size)
                {
#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        std::cout
                            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                            << ". client-client. sent " << (int)size
                            << std::endl;
                    }
#endif

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
#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
                        {
                            std::lock_guard<std::mutex> lk(mtx);
                            std::cout
                                << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                                << ". client-client. read " << (int)size
                                << std::endl;
                        }
#endif

                        if (code != SystemError::noError)
                            return clientResults.push(code);

                        EXPECT_GT(size, (size_t)0);
                        EXPECT_STREQ(buffer->data(), testMessage.data());

                        buffer->resize(0);
                        testClient->readAsyncAtLeast(
                            buffer.get(), testMessage.size(),
                            [&, buffer](SystemError::ErrorCode code, size_t size)
                            {
#ifdef SIMPLE_SOCKET_TEST_HELPER_DEBUG_OUTPUT
                                {
                                    std::lock_guard<std::mutex> lk(mtx);
                                    std::cout
                                        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()
                                        << ". client-client22. read " << (int)size
                                        << std::endl;
                                }
#endif

                                EXPECT_EQ(buffer->size(), 0);
                                EXPECT_EQ(size, (size_t)0);
                                clientResults.push(code);
                            });
                    });
                });
        });

        ASSERT_EQ(serverResults.pop(), SystemError::noError) << i; // accept
        ASSERT_EQ(clientResults.pop(), SystemError::noError) << i; // send
        ASSERT_EQ(serverResults.pop(), SystemError::noError) << i; // recv
    }
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketMultiConnect(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none,
    int clientCount = kClientCount)
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
    ASSERT_TRUE(server->listen(clientCount)) << lastError();

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

    connectNewClients(clientCount);

    for (int i = 0; i < clientCount; ++i)
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
    ASSERT_TRUE(client->bind(SocketAddress::anyAddress));
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
    SocketAddress endpointToBindToTo = SocketAddress::anyPrivateAddress;

    // Takes amazingly long with UdtSocket.
    const auto repeatCount = useAsyncPriorSync ? 5 : 14;
    for (int i = 0; i < repeatCount; ++i)
    {
        auto server = serverMaker();
        nx::utils::promise<SocketAddress> promise;
        nx::utils::thread serverThread(
            &syncSocketServerMainFunc<decltype(server)>,
            endpointToBindToTo,
            useAsyncPriorSync ? kTestMessage : Buffer(),
            1,
            serverMaker(),
            &promise,
            ActionOnReadWriteError::ignore);  //this test shuts down socket, so any server socket operation may fail 
                    //at any moment, so ignoring errors in serverThread.
                    //Testing that shutdown interrupts client socket operations

        auto serverAddress = promise.get_future().get();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!endpointToConnectTo)
            endpointToConnectTo = std::move(serverAddress);

        // TODO: #mux Figure out why it fails on UdtSocket when address changes
        if (endpointToBindToTo == SocketAddress::anyPrivateAddress)
            endpointToBindToTo = std::move(serverAddress);

        auto client = clientMaker();
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
                //while (client->recv(readBuffer.data(), readBuffer.size(), 0) > 0);

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

        //shutting down socket
        //if (i == 0)
        {
            //giving client thread some time to call client->recv
            std::this_thread::sleep_for(std::chrono::milliseconds(nx::utils::random::number(0, 500)));
            //testing that shutdown interrupts recv call
            client->shutdown();
        }

        ASSERT_EQ(
            std::future_status::ready,
            recvExitedPromise.get_future().wait_for(std::chrono::seconds(1)));

        using namespace std::chrono;
        serverThread.join();
        clientThread.join();
    }
}

namespace {
void pleaseStopSync(std::unique_ptr<QnStoppableAsync> socket) {
    socket->pleaseStopSync();
}
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleAsync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    socketSimpleAsync(
        serverMaker,
        clientMaker,
        endpointToConnectTo,
        testMessage,
        clientCount,
        pleaseStopSync);
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleTrueAsync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    nx::utils::TestSyncQueue<bool> stopQueue;
    socketSimpleAsync<ServerSocketMaker, ClientSocketMaker>(
        serverMaker, clientMaker, endpointToConnectTo, testMessage, clientCount,
        [&](std::unique_ptr<QnStoppableAsync> socket)
        {
            QnStoppableAsync::pleaseStop([&](){ stopQueue.push(true); },
                                         std::move(socket));
        });

    for (auto i = 0; i < (clientCount * 2) + 1; ++i)
        EXPECT_EQ(stopQueue.pop(), true);

    ASSERT_TRUE(stopQueue.isEmpty());
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleAcceptMixed(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    boost::optional<SocketAddress> endpointToConnectTo = boost::none)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen(kClientCount));

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

    pleaseStopSync(std::move(client));
    pleaseStopSync(std::move(server));
}

template<typename ClientSocketMaker>
void socketSingleAioThread(
    const ClientSocketMaker& clientMaker,
    int clientCount = kClientCount)
{
    aio::AbstractAioThread* aioThread(nullptr);
    std::vector<decltype(clientMaker())> sockets;
    nx::utils::TestSyncQueue<nx::utils::thread::id> threadIdQueue;

    for (auto i = 0; i < clientCount; ++i)
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
    for (auto i = 0; i < clientCount; ++i)
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
void socketConnectCancel(
    const ClientSocketMaker& clientMaker, StopType stopType)
{
    auto client = clientMaker();
    ASSERT_TRUE(client->setNonBlockingMode(true));
    client->connectAsync(
        SocketAddress("ya.ru:80"),
        [](SystemError::ErrorCode) { NX_CRITICAL(false); });

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

    const auto start = std::chrono::system_clock::now();
    EXPECT_EQ(server->accept(), nullptr);
    EXPECT_EQ(SystemError::getLastOSErrorCode(), SystemError::timedOut);
    EXPECT_LT(std::chrono::system_clock::now() - start, timeout * 2);
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
    const auto start = std::chrono::system_clock::now();
    server->acceptAsync([&](SystemError::ErrorCode /*code*/,
                            AbstractStreamSocket* /*socket*/)
    {
        if (deleteInIoThread)
            server.reset();

        serverResults.push(SystemError::timedOut);
    });

    EXPECT_EQ(serverResults.pop(), SystemError::timedOut);
    EXPECT_TRUE(std::chrono::system_clock::now() - start < timeout * 2);
    if (server) pleaseStopSync(std::move(server));
}

template<typename ServerSocketMaker>
void socketAcceptCancel(
    const ServerSocketMaker& serverMaker, StopType stopType)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen(5));

    for (auto i = 0; i < kClientCount; ++i)
    {
        server->acceptAsync(
            [&](SystemError::ErrorCode, AbstractStreamSocket*) { NX_CRITICAL(false); });

        if (i) std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));
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
    Type(Name, ConnectCancelIo) \
        { nx::network::test::socketConnectCancel(mkClient, StopType::cancelIo); } \
    Type(Name, ConnectPleaseStop) \
        { nx::network::test::socketConnectCancel(mkClient, StopType::pleaseStop); } \

#define NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, SimpleAcceptMixed) \
        { nx::network::test::socketSimpleAcceptMixed(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, AcceptTimeoutSync) \
        { nx::network::test::socketAcceptTimeoutSync(mkServer); } \
    Type(Name, AcceptTimeoutAsync) \
        { nx::network::test::socketAcceptTimeoutAsync(mkServer, false); } \
    Type(Name, AcceptTimeoutAsyncIoDelete) \
        { nx::network::test::socketAcceptTimeoutAsync(mkServer, true); } \
    Type(Name, AcceptCancelIo) \
        { nx::network::test::socketAcceptCancel(mkServer, StopType::cancelIo); } \
    Type(Name, AcceptPleaseStop) \
        { nx::network::test::socketAcceptCancel(mkServer, StopType::pleaseStop); } \

#define NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, SimpleSync) \
        { nx::network::test::socketSimpleSync(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, SimpleSyncFlags) \
        { nx::network::test::socketSimpleSyncFlags(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, SimpleAsync) \
        { nx::network::test::socketSimpleAsync(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, SimpleTrueAsync) \
        { nx::network::test::socketSimpleTrueAsync(mkServer, mkClient, endpointToConnectTo); } \
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

