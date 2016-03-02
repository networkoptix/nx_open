
#ifndef SIMPLE_SOCKET_TEST_HELPER_H
#define SIMPLE_SOCKET_TEST_HELPER_H

#include <iostream>

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



template<typename ServerSocketMaker>
void syncSocketServerMainFunc(
    const SocketAddress& endpointToBindTo,
    const boost::optional<QByteArray> testMessage,
    int clientCount,
    ServerSocketMaker serverMaker,
    std::promise<void>* startedPromise)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));

    ASSERT_TRUE(server->bind(endpointToBindTo))
        << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen(clientCount))
        << SystemError::getLastOSErrorText().toStdString();

    for (int i = clientCount; i > 0; --i)
    {
        std::unique_ptr<AbstractStreamSocket> client;
        if (startedPromise)
        {
            //we must trigger startedPromise after actual accept call: UDT requirement
            std::promise<
                std::pair<SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>>
            > acceptedPromise;
            server->acceptAsync(
                [&acceptedPromise](
                    SystemError::ErrorCode errorCode,
                    AbstractStreamSocket* socket)
                {
                    acceptedPromise.set_value(
                        std::make_pair(
                            errorCode,
                            std::unique_ptr<AbstractStreamSocket>(socket)));
                });

            startedPromise->set_value();
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

        if (!testMessage)
            continue;

        const auto incomingMessage = readNBytes(client.get(), testMessage->size());
        ASSERT_TRUE(!incomingMessage.isEmpty())
            << SystemError::getLastOSErrorText().toStdString();

        ASSERT_EQ(*testMessage, incomingMessage);

        const int bytesSent = client->send(*testMessage);
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
    const SocketAddress& endpointToBindTo = kServerAddress,
    const SocketAddress& endpointToConnectTo = kServerAddress,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    std::promise<void> promise;
    std::thread serverThread(
        syncSocketServerMainFunc<ServerSocketMaker>,
        endpointToBindTo,
        testMessage,
        clientCount,
        serverMaker,
        &promise);

    promise.get_future().wait();

    std::thread clientThread([&endpointToConnectTo, &testMessage,
                              clientCount, &clientMaker]()
    {
        for (int i = clientCount; i > 0; --i)
        {
            auto client = clientMaker();
            EXPECT_TRUE(client->connect(endpointToConnectTo, kTestTimeout.count()))
                << SystemError::getLastOSErrorText().toStdString();
            ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
            ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

            EXPECT_EQ(
                testMessage.size(),
                client->send(testMessage.constData(), testMessage.size()))
                    << SystemError::getLastOSErrorText().toStdString();

            const auto incomingMessage = readNBytes(client.get(), testMessage.size());
            ASSERT_TRUE(!incomingMessage.isEmpty())
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
    void socketMultiConnect(
        const ServerSocketMaker& serverMaker,
        const ClientSocketMaker& clientMaker,
        const SocketAddress& endpoint = kServerAddress,
        int clientCount = kClientCount)
{
    static const std::chrono::milliseconds timeout(1500);

    nx::SyncQueue< SystemError::ErrorCode > acceptResults;
    nx::SyncQueue< SystemError::ErrorCode > connectResults;

    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedSockets;
    std::vector<std::unique_ptr<AbstractStreamSocket>> connectedSockets;

    auto server = serverMaker();
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(timeout.count()));
    ASSERT_TRUE(server->bind(endpoint)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen(clientCount)) << SystemError::getLastOSErrorText().toStdString();

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
void shutdownSocket(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    const SocketAddress& endpointToBindTo = kServerAddress,
    const SocketAddress& endpointToConnectTo = kServerAddress)
{
    std::promise<void> promise;
    std::thread serverThread(
        syncSocketServerMainFunc<ServerSocketMaker>,
        endpointToBindTo,
        boost::none,
        1,
        serverMaker,
        &promise);

    promise.get_future().wait();

    auto client = clientMaker();
    ASSERT_TRUE(client->setRecvTimeout(10 * 1000));   //10 seconds
    EXPECT_TRUE(client->connect(endpointToConnectTo, kTestTimeout.count()));
    std::atomic<bool> recvExited(false);
    std::thread clientThread(
        [&client, &recvExited]()
        {
            nx::Buffer readBuffer;
            readBuffer.resize(4096);
            client->recv(readBuffer.data(), readBuffer.size(), 0);
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
    ASSERT_TRUE(server->listen(kClientCount));

    // no clients yet
    ASSERT_EQ(server->accept(), nullptr);
    ASSERT_EQ(SystemError::getLastOSErrorCode(), SystemError::wouldBlock);

    auto client = clientMaker();
    ASSERT_TRUE(client->setNonBlockingMode(true));
    client->connectAsync(kServerAddress, [&](SystemError::ErrorCode /*code*/){});

    // let the client get in the server listen queue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_NE(server->accept(), nullptr)
        << SystemError::getLastOSErrorText().toStdString();

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
    EXPECT_LT(std::chrono::system_clock::now() - start, timeout * 2);
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

} // namespace test
} // namespace network
} // namespace nx

#define NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient)     \
    Type(Name, SingleAioThread)                                                 \
        { nx::network::test::socketSingleAioThread(mkClient); }                 \
    Type(Name, Shutdown)                                                        \
        { nx::network::test::shutdownSocket(mkServer, mkClient); }              \

#define NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient)     \
    Type(Name, SimpleAcceptMixed)                                               \
        { nx::network::test::socketSimpleAcceptMixed(mkServer, mkClient); }     \
    Type(Name, AcceptTimeoutSync)                                               \
        { nx::network::test::socketAcceptTimeoutSync(mkServer); }               \
    Type(Name, AcceptTimeoutAsync)                                              \
        { nx::network::test::socketAcceptTimeoutAsync(mkServer); }              \

#define NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient)  \
    Type(Name, SimpleSync)                                                      \
        { nx::network::test::socketSimpleSync(mkServer, mkClient); }            \
    Type(Name, SimpleAsync)                                                     \
        { nx::network::test::socketSimpleAsync(mkServer, mkClient); }           \
    Type(Name, SimpleTrueAsync)                                                 \
        { nx::network::test::socketSimpleTrueAsync(mkServer, mkClient); }       \
    Type(Name, SimpleMultiConnect)                                              \
        { nx::network::test::socketMultiConnect(mkServer, mkClient); }          \

#define NX_NETWORK_CLIENT_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient)  \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient)     \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient)  \

#define NX_NETWORK_SERVER_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient)  \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient)     \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient)  \

#define NX_NETWORK_BOTH_SOCKETS_TEST_CASE(Type, Name, mkServer, mkClient)   \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient)     \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient)     \
    NX_NETWORK_TRANSMIT_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient)  \

#endif  //SIMPLE_SOCKET_TEST_HELPER_H
