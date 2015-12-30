
#ifndef SIMPLE_SOCKET_TEST_HELPER_H
#define SIMPLE_SOCKET_TEST_HELPER_H

#include <utils/thread/sync_queue.h>
#include <utils/common/systemerror.h>
#include <utils/common/stoppable.h>
#include <nx/network/abstract_socket.h>

// Template multitype socket tests to ensure that every common_ut run checks
// TCP and UDT basic functionality

namespace /* anonimous */ {

const SocketAddress kServerAddress("localhost:12345");
const QByteArray kTestMessage("Ping");
const size_t kClientCount(3);

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleSync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    const SocketAddress& serverAddress = kServerAddress,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    std::thread serverThread([&serverAddress, &testMessage,
                             clientCount, &serverMaker]()
    {
        auto server = serverMaker();
        ASSERT_TRUE(server->setReuseAddrFlag(true));

        ASSERT_TRUE(server->bind(serverAddress))
                << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(server->listen(clientCount))
                << SystemError::getLastOSErrorText().toStdString();

        for (int i = clientCount; i > 0; --i)
        {
            static const int BUF_SIZE = 128;

            QByteArray buffer(BUF_SIZE, char(0));
            std::unique_ptr< AbstractStreamSocket > client(server->accept());
            ASSERT_TRUE(client.get())
                    << SystemError::getLastOSErrorText().toStdString();

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

            EXPECT_STREQ(testMessage.data(), buffer.data());
        }
    });

    // give the server some time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread clientThread([&serverAddress, &testMessage,
                              clientCount, &clientMaker]()
    {
        for (int i = clientCount; i > 0; --i)
        {
            auto client = clientMaker();
            EXPECT_TRUE(client->connect(serverAddress, 500));
            EXPECT_EQ(client->send(testMessage.data(), testMessage.size() + 1),
                testMessage.size() + 1);
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
    const SocketAddress& serverAddress,
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
    ASSERT_TRUE(server->bind(serverAddress)) << SystemError::getLastOSErrorText().toStdString();
    ASSERT_TRUE(server->listen(clientCount)) << SystemError::getLastOSErrorText().toStdString();

    QByteArray serverBuffer;
    serverBuffer.reserve(128);
    std::unique_ptr< AbstractStreamSocket > client;
    std::function< void(SystemError::ErrorCode, AbstractStreamSocket*) > accept
        = [&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
        serverResults.push(code);
        if (code != SystemError::noError)
            return;

        client.reset(socket);
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

            stopSocket( std::move( client ) );
            server->acceptAsync(accept);
            serverResults.push(code);
        });
    };

    server->acceptAsync(accept);

    auto testClient = clientMaker();
    ASSERT_TRUE(testClient->setNonBlockingMode(true));
    ASSERT_TRUE(testClient->setSendTimeout(5000));

    //have to introduce wait here since subscribing to socket events (acceptAsync)
    //  takes some time and UDT ignores connections received before first accept call
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    QByteArray clientBuffer;
    clientBuffer.reserve(128);
    testClient->connectAsync(serverAddress, [&](SystemError::ErrorCode code)
    {
        EXPECT_EQ(code, SystemError::noError);
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
    stopSocket(std::move(server));
}

void pleaseStopSync(std::unique_ptr<QnStoppableAsync> socket) {
    socket->pleaseStopSync();
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSimpleAsync(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    const SocketAddress& serverAddress = kServerAddress,
    const QByteArray& testMessage = kTestMessage,
    int clientCount = kClientCount)
{
    socketSimpleAsync(serverMaker, clientMaker, serverAddress, testMessage,
                      clientCount, pleaseStopSync);
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
        serverMaker, clientMaker, serverAddress, testMessage, clientCount,
        [&](std::unique_ptr<QnStoppableAsync> socket)
        {
            QnStoppableAsync::pleaseStop([&](){ stopQueue.push(true); },
                                         std::move(socket));
        });

    for (auto i = 0; i < clientCount; ++i)
        EXPECT_EQ( stopQueue.pop(), true );

    ASSERT_TRUE( stopQueue.isEmpty() );
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

} // namespace /* anonimous */

#endif  //SIMPLE_SOCKET_TEST_HELPER_H
