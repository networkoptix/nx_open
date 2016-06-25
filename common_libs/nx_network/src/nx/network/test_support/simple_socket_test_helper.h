
#ifndef SIMPLE_SOCKET_TEST_HELPER_H
#define SIMPLE_SOCKET_TEST_HELPER_H

#include <iostream>

#include <boost/optional.hpp>

#include <utils/thread/sync_queue.h>
#include <utils/common/systemerror.h>
#include <utils/common/stoppable.h>
#include <nx/network/abstract_socket.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/log/log.h>

// Template multitype socket tests to ensure that every common_ut run checks
// TCP and UDT basic functionality


namespace nx {
namespace network {
namespace test {

namespace /* anonimous */ {

const SocketAddress kAnyPrivateAddress("127.0.0.1:0");
const QByteArray kTestMessage("Ping");
const int kClientCount(10);
const std::chrono::milliseconds kTestTimeout(3000);

}

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
            return QByteArray();

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
    nx::utils::promise<SocketAddress>* startedPromise)
{
    ASSERT_TRUE(server->setReuseAddrFlag(true));

    ASSERT_TRUE(server->bind(endpointToBindTo))
        << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen(clientCount))
        << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->setRecvTimeout(60 * 1000))
        << SystemError::getLastOSErrorText().toStdString();

    for (int i = clientCount; i > 0; --i)
    {
        std::unique_ptr<AbstractStreamSocket> client;
        if (startedPromise)
        {
            //we must trigger startedPromise after actual accept call: UDT requirement
            nx::utils::promise<
                std::pair<SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>>
            > acceptedPromise;
            server->acceptAsync(
                [&server, &acceptedPromise](
                    SystemError::ErrorCode errorCode,
                    AbstractStreamSocket* socket)
                {
                    server->post(
                        [&acceptedPromise, errorCode, socket]()
                        {
                            acceptedPromise.set_value(
                                std::make_pair(
                                    errorCode,
                                    std::unique_ptr<AbstractStreamSocket>(socket)));
                        });
                });

            auto serverAddress = server->getLocalAddress();
            NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
            startedPromise->set_value(std::move(serverAddress));
            startedPromise = nullptr;

            auto acceptResult = acceptedPromise.get_future().get();
            client = std::move(acceptResult.second);
            if (acceptResult.first != SystemError::noError)
            {
                SystemError::setLastErrorCode(acceptResult.first);
            }
            else
            {
                ASSERT_NE(nullptr, client);
                ASSERT_TRUE(client->setNonBlockingMode(false))
                    << SystemError::getLastOSErrorText().toStdString();
            }
        }
        else
        {
            client.reset(server->accept());
        }
        
        ASSERT_TRUE(client.get())
            << SystemError::getLastOSErrorText().toStdString() << " on " << i;
        ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
        ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

        if (testMessage.isEmpty())
            continue;

        const auto incomingMessage = readNBytes(client.get(), testMessage.size());
        ASSERT_TRUE(!incomingMessage.isEmpty())
            << SystemError::getLastOSErrorText().toStdString();

        ASSERT_EQ(testMessage, incomingMessage);

        const int bytesSent = client->send(testMessage);
        ASSERT_NE(-1, bytesSent) << SystemError::getLastOSErrorText().toStdString();

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
    const SocketAddress& endpointToBindTo = kAnyPrivateAddress,
    SocketAddress endpointToConnectTo = kAnyPrivateAddress,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    auto server = serverMaker();
    nx::utils::promise<SocketAddress> promise;
    nx::utils::thread serverThread(
        &syncSocketServerMainFunc<decltype(server)>,
        endpointToBindTo,
        testMessage,
        clientCount,
        std::move(server),
        &promise);

    auto serverAddress = promise.get_future().get();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (endpointToConnectTo == kAnyPrivateAddress)
        endpointToConnectTo = std::move(serverAddress);

    nx::utils::thread clientThread(
        [endpointToConnectTo, &testMessage, clientCount, &clientMaker]()
        {
            for (int i = clientCount; i > 0; --i)
            {
                auto client = clientMaker();
                EXPECT_TRUE(client->connect(
                    endpointToConnectTo, kTestTimeout.count()))
                        << SystemError::getLastOSErrorText().toStdString();

                ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
                ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

                EXPECT_EQ(
                    testMessage.size(),
                    client->send(testMessage.constData(), testMessage.size()))
                        << SystemError::getLastOSErrorText().toStdString();

                const auto incomingMessage = readNBytes(
                    client.get(), testMessage.size());

                ASSERT_TRUE(!incomingMessage.isEmpty()) << i << ": "
                    << SystemError::getLastOSErrorText().toStdString();

                ASSERT_EQ(testMessage, incomingMessage);
                client.reset();
            }
        });

    serverThread.join();
    clientThread.join();
}

template<typename ServerSocketMaker, typename ClientSocketMaker,
         typename StopSocketFunc>
void socketSimpleAsync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    const SocketAddress& endpointToBindTo,
    SocketAddress endpointToConnectTo,
    const QByteArray& testMessage,
    int clientCount,
    StopSocketFunc stopSocket)
{
    nx::TestSyncQueue< SystemError::ErrorCode > serverResults;
    nx::TestSyncQueue< SystemError::ErrorCode > clientResults;

    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(5000));
    ASSERT_TRUE(server->bind(endpointToBindTo)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen(clientCount)) << SystemError::getLastOSErrorText().toStdString();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (endpointToConnectTo == kAnyPrivateAddress)
        endpointToConnectTo = std::move(serverAddress);

    QByteArray serverBuffer;
    serverBuffer.reserve(128);
    std::vector<std::unique_ptr<AbstractStreamSocket>> clients;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> acceptor
        = [&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
        serverResults.push(code);
        if (code != SystemError::noError)
            return;

        clients.emplace_back(socket);
        auto& client = clients.back();
        ASSERT_TRUE(client->setNonBlockingMode(true));
        client->readAsyncAtLeast(
            &serverBuffer, testMessage.size(),
            [&](SystemError::ErrorCode code, size_t size)
            {
                if (code == SystemError::noError)
                {
                    EXPECT_GT(size, (size_t)0);
                    EXPECT_STREQ(serverBuffer.data(), testMessage.data());
                    serverBuffer.resize(0);
                }

                stopSocket(std::move(client));
                serverResults.push(code);
            });

        server->acceptAsync(acceptor);
    };

    server->acceptAsync(acceptor);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (int i = clientCount; i > 0; --i)
    {
        auto testClient = clientMaker();
        ASSERT_TRUE(testClient->setNonBlockingMode(true));
        ASSERT_TRUE(testClient->setSendTimeout(3000));

        QByteArray clientBuffer;
        clientBuffer.reserve(128);
        testClient->connectAsync(endpointToConnectTo, [&](SystemError::ErrorCode code)
        {
            ASSERT_EQ(code, SystemError::noError);
            testClient->sendAsync(
                testMessage,
                [&](SystemError::ErrorCode code, size_t size)
                {
                    clientResults.push(code);
                    if (code != SystemError::noError)
                        return;

                    EXPECT_EQ(code, SystemError::noError);
                    EXPECT_EQ(size, (size_t)testMessage.size());
                });
        });

        ASSERT_EQ(serverResults.pop(), SystemError::noError) << i; // accept
        ASSERT_EQ(clientResults.pop(), SystemError::noError) << i; // send
        ASSERT_EQ(serverResults.pop(), SystemError::noError) << i; // recv

        stopSocket(std::move(testClient));
    }

    stopSocket(std::move(server));
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
    void socketMultiConnect(
        const ServerSocketMaker& serverMaker,
        const ClientSocketMaker& clientMaker,
        SocketAddress endpoint = kAnyPrivateAddress,
        int clientCount = kClientCount)
{
    static const std::chrono::milliseconds timeout(1500);

    nx::TestSyncQueue< SystemError::ErrorCode > acceptResults;
    nx::TestSyncQueue< SystemError::ErrorCode > connectResults;

    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedSockets;
    std::vector<std::unique_ptr<AbstractStreamSocket>> connectedSockets;

    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(timeout.count()));
    ASSERT_TRUE(server->bind(endpoint)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen(clientCount)) << SystemError::getLastOSErrorText().toStdString();

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (endpoint == kAnyPrivateAddress)
        endpoint = std::move(serverAddress);

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
                endpoint, 
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
void socketShutdown(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    bool useAsyncPriorSync,
    SocketAddress endpointToBindTo = kAnyPrivateAddress,
    SocketAddress endpointToConnectTo = kAnyPrivateAddress)
{
    // Taks amazingly long with UdtSocket
    const auto repeatCount = useAsyncPriorSync ? 5 : 14;
    for (int i = 0; i < repeatCount; ++i)
    {
        auto server = serverMaker();
        nx::utils::promise<SocketAddress> promise;
        nx::utils::thread serverThread(
            &syncSocketServerMainFunc<decltype(server)>,
            endpointToBindTo,
            useAsyncPriorSync ? kTestMessage : Buffer(),
            1,
            serverMaker(),
            &promise);

        auto serverAddress = promise.get_future().get();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (endpointToConnectTo == kAnyPrivateAddress)
            endpointToConnectTo = std::move(serverAddress);

        // TODO: #mux Figure out why it fails on UdtSocket when address changes
        if (endpointToBindTo == kAnyPrivateAddress)
            endpointToBindTo = std::move(serverAddress);

        auto client = clientMaker();
        ASSERT_TRUE(client->setRecvTimeout(10 * 1000));   //10 seconds

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
                        endpointToConnectTo,
                        [&](SystemError::ErrorCode code)
                        {
                            ASSERT_EQ(code, SystemError::noError);
                            client->sendAsync(
                                kTestMessage,
                                [&](SystemError::ErrorCode code, size_t /*size*/)
                                {
                                    ASSERT_EQ(code, SystemError::noError);
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
                    client->connect(endpointToConnectTo, kTestTimeout.count());
                }

                nx::Buffer readBuffer;
                readBuffer.resize(4096);
                while (client->recv(readBuffer.data(), readBuffer.size(), 0) > 0);
                recvExitedPromise.set_value();
            });

        if (useAsyncPriorSync)
            testReadyPromise.get_future().wait();

        //shutting down socket
        //if (i == 0)
        {
            //giving client thread some time to call client->recv
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 500));
            //testing that shutdown interrupts recv call
            client->shutdown();
        }
        ASSERT_EQ(
            std::future_status::ready,
            recvExitedPromise.get_future().wait_for(std::chrono::seconds(1)));

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
    const SocketAddress& endpointToBindTo = kAnyPrivateAddress,
    const SocketAddress& endpointToConnectTo = kAnyPrivateAddress,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    socketSimpleAsync(
        serverMaker,
        clientMaker,
        endpointToBindTo,
        endpointToConnectTo,
        testMessage,
        clientCount,
        pleaseStopSync);
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleTrueAsync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    const SocketAddress& serverAddress = kAnyPrivateAddress,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    nx::TestSyncQueue<bool> stopQueue;
    socketSimpleAsync<ServerSocketMaker, ClientSocketMaker>(
        serverMaker, clientMaker, serverAddress, serverAddress, testMessage, clientCount,
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
    SocketAddress endpoint = kAnyPrivateAddress)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(endpoint));
    ASSERT_TRUE(server->listen(kClientCount));

    auto serverAddress = server->getLocalAddress();
    NX_LOG(lm("Server address: %1").arg(serverAddress.toString()), cl_logDEBUG1);
    if (endpoint == kAnyPrivateAddress)
        endpoint = std::move(serverAddress);

    // no clients yet
    std::unique_ptr<AbstractStreamSocket> accepted0(server->accept());
    ASSERT_EQ(accepted0, nullptr);
    ASSERT_EQ(SystemError::getLastOSErrorCode(), SystemError::wouldBlock);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto client = clientMaker();
    ASSERT_TRUE(client->setSendTimeout(1000));
    ASSERT_TRUE(client->setNonBlockingMode(true));
    nx::utils::promise<SystemError::ErrorCode> connectionEstablishedPromise;
    client->connectAsync(
        endpoint,
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

    std::unique_ptr<AbstractStreamSocket> accepted1(server->accept());
    ASSERT_NE(accepted1, nullptr)<< SystemError::getLastOSErrorText().toStdString();

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
    nx::TestSyncQueue<nx::utils::thread::id> threadIdQueue;

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
            connectCompletedPromise.set_value(errorCode);
            if (deleteInIoThread)
                client.reset();
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
    const SocketAddress& serverAddress = kAnyPrivateAddress,
    std::chrono::milliseconds timeout = kTestTimeout)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(timeout.count()));
    ASSERT_TRUE(server->bind(serverAddress));
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
    const SocketAddress& serverAddress = kAnyPrivateAddress,
    std::chrono::milliseconds timeout = kTestTimeout)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(timeout.count()));
    ASSERT_TRUE(server->bind(serverAddress));
    ASSERT_TRUE(server->listen(5));

    nx::TestSyncQueue< SystemError::ErrorCode > serverResults;
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
    const ServerSocketMaker& serverMaker, StopType stopType,
    const SocketAddress& serverAddress = kAnyPrivateAddress)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(serverAddress));
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

#define NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient) \
    Type(Name, SingleAioThread) \
        { nx::network::test::socketSingleAioThread(mkClient); } \
    Type(Name, Shutdown) \
        { nx::network::test::socketShutdown(mkServer, mkClient, false); } \
    Type(Name, ShutdownAfterAsync) \
        { nx::network::test::socketShutdown(mkServer, mkClient, true); } \
    Type(Name, ConnectToBadAddress) \
        { nx::network::test::socketConnectToBadAddress(mkClient, false); } \
    Type(Name, ConnectToBadAddressIoDelete) \
        { nx::network::test::socketConnectToBadAddress(mkClient, true); } \
    Type(Name, ConnectCancelIo) \
        { nx::network::test::socketConnectCancel(mkClient, StopType::cancelIo); } \
    Type(Name, ConnectPleaseStop) \
        { nx::network::test::socketConnectCancel(mkClient, StopType::pleaseStop); } \

#define NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient) \
    Type(Name, SimpleAcceptMixed) \
        { nx::network::test::socketSimpleAcceptMixed(mkServer, mkClient); } \
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

#define NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient) \
    Type(Name, SimpleSync) \
        { nx::network::test::socketSimpleSync(mkServer, mkClient); } \
    Type(Name, SimpleAsync) \
        { nx::network::test::socketSimpleAsync(mkServer, mkClient); } \
    Type(Name, SimpleTrueAsync) \
        { nx::network::test::socketSimpleTrueAsync(mkServer, mkClient); } \
    Type(Name, SimpleMultiConnect) \
        { nx::network::test::socketMultiConnect(mkServer, mkClient); } \

#define NX_NETWORK_CLIENT_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient) \

#define NX_NETWORK_SERVER_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient) \

#define NX_NETWORK_BOTH_SOCKETS_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient) \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient) \

#endif  //SIMPLE_SOCKET_TEST_HELPER_H
