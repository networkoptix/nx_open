
#ifndef SIMPLE_SOCKET_TEST_HELPER_H
#define SIMPLE_SOCKET_TEST_HELPER_H

#include <boost/optional.hpp>

#include <utils/thread/sync_queue.h>
#include <utils/common/systemerror.h>
#include <utils/common/stoppable.h>
#include <nx/network/abstract_socket.h>

// Template multitype socket tests to ensure that every common_ut run checks
// TCP and UDT basic functionality


namespace nx {
namespace network {
namespace test {

namespace /* anonimous */ {

const SocketAddress kServerAddress("127.0.0.1:12345");
const QByteArray kTestMessage("Ping");
const int kClientCount(3);
const std::chrono::milliseconds kTestTimeout(500);

}

template<typename ServerSocketMaker>
void syncSocketServerMainFunc(
    const SocketAddress& endpointToBindTo,
    const boost::optional<QByteArray> testMessage,
    int clientCount,
    ServerSocketMaker serverMaker)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));

    ASSERT_TRUE(server->bind(endpointToBindTo))
        << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen(clientCount))
        << SystemError::getLastOSErrorText().toStdString();

    for (int i = clientCount; i > 0; --i)
    {
        static const int BUF_SIZE = 128;

        QByteArray buffer(BUF_SIZE, char(0));
        std::unique_ptr<AbstractStreamSocket> client(server->accept());
        ASSERT_TRUE(client.get())
            << SystemError::getLastOSErrorText().toStdString() << " on " << i;

        int bufDataSize = 0;
        for (;;)
        {
            const auto bytesRead = client->recv(
                buffer.data() + bufDataSize,
                buffer.size() - bufDataSize);
            ASSERT_NE(-1, bytesRead)
                << SystemError::getLastOSErrorText().toStdString();

            if (bytesRead == 0)
                break;  //connection closed
            bufDataSize += bytesRead;
        }

        if (testMessage)
            EXPECT_STREQ(testMessage->constData(), buffer.constData());
    }
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleSync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    const SocketAddress& endpointToBindTo = kServerAddress,
    const SocketAddress& endpointToConnectTo = kServerAddress,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    std::thread serverThread(
        syncSocketServerMainFunc<ServerSocketMaker>,
        endpointToBindTo,
        testMessage,
        clientCount,
        serverMaker);

    // give the server some time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread clientThread([&endpointToConnectTo, &testMessage,
                              clientCount, &clientMaker]()
    {
        for (int i = clientCount; i > 0; --i)
        {
            auto client = clientMaker();
            EXPECT_TRUE(client->connect(endpointToConnectTo, 500));
            EXPECT_EQ(
                client->send(testMessage.constData(), testMessage.size() + 1),
                testMessage.size() + 1) << SystemError::getLastOSErrorText().toStdString();
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
    const SocketAddress& endpointToConnectTo,
    const QByteArray& testMessage,
    int clientCount,
    StopSocketFunc stopSocket)
{
    nx::SyncQueue< SystemError::ErrorCode > serverResults;
    nx::SyncQueue< SystemError::ErrorCode > clientResults;

    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(5000));
    ASSERT_TRUE(server->bind(endpointToBindTo)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen(clientCount)) << SystemError::getLastOSErrorText().toStdString();

    QByteArray serverBuffer;
    serverBuffer.reserve(128);
    std::vector<std::unique_ptr<AbstractStreamSocket>> clients;
    std::function< void(SystemError::ErrorCode, AbstractStreamSocket*) > acceptor
        = [&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
        serverResults.push(code);
        if (code != SystemError::noError)
            return;

        clients.emplace_back(socket);
        auto& client = clients.back();
        ASSERT_TRUE(client->setNonBlockingMode(true));
        client->readSomeAsync(&serverBuffer, [&](SystemError::ErrorCode code,
            size_t size)
        {
            if (code == SystemError::noError)
            {
                EXPECT_GT(size, 0);
                EXPECT_STREQ(serverBuffer.data(), testMessage.data());
                serverBuffer.resize(0);
            }

            stopSocket(std::move(client));
            serverResults.push(code);
        });

        server->acceptAsync(acceptor);
    };

    server->acceptAsync(acceptor);
    for (int i = clientCount; i > 0; --i)
    {
        auto testClient = clientMaker();
        ASSERT_TRUE(testClient->setNonBlockingMode(true));
        ASSERT_TRUE(testClient->setSendTimeout(5000));

        // have to introduce wait here since subscribing to socket events
        // (acceptAsync) takes some time and UDT ignores connections received
        // before first accept call
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        QByteArray clientBuffer;
        clientBuffer.reserve(128);
        testClient->connectAsync(endpointToConnectTo, [&](SystemError::ErrorCode code)
        {
            ASSERT_EQ(code, SystemError::noError);
            testClient->sendAsync(testMessage, [&](SystemError::ErrorCode code,
                size_t size)
            {
                clientResults.push(code);
                if (code != SystemError::noError)
                    return;

                EXPECT_EQ(code, SystemError::noError);
                EXPECT_EQ(size, testMessage.size());
            });
        });

        ASSERT_EQ(serverResults.pop(), SystemError::noError); // accept
        ASSERT_EQ(clientResults.pop(), SystemError::noError); // send
        ASSERT_EQ(serverResults.pop(), SystemError::noError); // recv

        stopSocket(std::move(testClient));
    }

    stopSocket(std::move(server));
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void shutdownSocket(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    const SocketAddress& endpointToBindTo = kServerAddress,
    const SocketAddress& endpointToConnectTo = kServerAddress)
{
    std::thread serverThread(
        syncSocketServerMainFunc<ServerSocketMaker>,
        endpointToBindTo,
        boost::none,
        1,
        serverMaker);

    // give the server some time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto client = clientMaker();
    ASSERT_TRUE(client->setRecvTimeout(10 * 1000));   //10 seconds
    EXPECT_TRUE(client->connect(endpointToConnectTo, 500));
    std::atomic<bool> recvExited(false);
    std::thread clientThread(
        [&client, &recvExited]()
        {
            nx::Buffer readBuffer;
            readBuffer.resize(4096);
            client->recv(readBuffer.data(), readBuffer.size());
            recvExited = true;
        });

    //giving client thread some time to start
    std::this_thread::sleep_for(std::chrono::seconds(1));
    //shutting down socket
    client->shutdown();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(recvExited.load());

    serverThread.join();
    clientThread.join();
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
    const SocketAddress& endpointToBindTo = kServerAddress,
    const SocketAddress& endpointToConnectTo = kServerAddress,
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
    const SocketAddress& serverAddress = kServerAddress,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    nx::SyncQueue<bool> stopQueue;
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
    const SocketAddress& serverAddress = kServerAddress)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(serverAddress));
    ASSERT_TRUE(server->listen(5));

    // no clients yet
    ASSERT_EQ(server->accept(), nullptr);
    ASSERT_EQ(SystemError::getLastOSErrorCode(), SystemError::wouldBlock);

    auto client = clientMaker();
    ASSERT_TRUE(client->setNonBlockingMode(true));
    client->connectAsync(kServerAddress, [&](SystemError::ErrorCode /*code*/){});

    // let the client get in the server listen queue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_NE(server->accept(), nullptr);

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
    nx::SyncQueue<std::thread::id> threadIdQueue;

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

    boost::optional<std::thread::id> aioThreadId;
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

template<typename ServerSocketMaker>
void socketAcceptTimeoutSync(
    const ServerSocketMaker& serverMaker,
    const SocketAddress& serverAddress = kServerAddress,
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
    EXPECT_TRUE(std::chrono::system_clock::now() - start < timeout * 2);
}

template<typename ServerSocketMaker>
void socketAcceptTimeoutAsync(
    const ServerSocketMaker& serverMaker,
    const SocketAddress& serverAddress = kServerAddress,
    std::chrono::milliseconds timeout = kTestTimeout)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(timeout.count()));
    ASSERT_TRUE(server->bind(serverAddress));
    ASSERT_TRUE(server->listen(5));

    nx::SyncQueue< SystemError::ErrorCode > serverResults;
    const auto start = std::chrono::system_clock::now();
    server->acceptAsync([&](SystemError::ErrorCode /*code*/,
                            AbstractStreamSocket* /*socket*/)
    {
        serverResults.push(SystemError::timedOut);
    });

    EXPECT_EQ(serverResults.pop(), SystemError::timedOut);
    EXPECT_TRUE(std::chrono::system_clock::now() - start < timeout * 2);
    pleaseStopSync(std::move(server));
}

#define NX_SIMPLE_SOCKET_TESTS_BASE(type, test, mkServer, mkClient)                 \
    type(test, SimpleSync)          { socketSimpleSync(mkServer, mkClient); }       \
    type(test, SimpleAsync)         { socketSimpleAsync(mkServer, mkClient); }      \
    type(test, SimpleTrueAsync)     { socketSimpleTrueAsync(mkServer, mkClient); }  \
    type(test, SingleAioThread)     { socketSingleAioThread(mkClient); }            \
    type(test, SimpleAcceptMixed)   { socketSimpleAcceptMixed(mkServer, mkClient); }\
    type(test, AcceptTimeoutSync)   { socketAcceptTimeoutSync(mkServer); }          \
    type(test, AcceptTimeoutAsync)  { socketAcceptTimeoutAsync(mkServer); }         \

#define NX_SIMPLE_SOCKET_TESTS(test, mkServer, mkClient) \
    NX_SIMPLE_SOCKET_TESTS_BASE(TEST, test, mkServer, mkClient)

#define NX_SIMPLE_SOCKET_TESTS_F(test, mkServer, mkClient) \
    NX_SIMPLE_SOCKET_TESTS_BASE(TEST_F, test, mkServer, mkClient)

}   //test
}   //network
}   //nx

#endif  //SIMPLE_SOCKET_TEST_HELPER_H
