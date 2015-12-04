
#ifndef SIMPLE_SOCKET_TEST_HELPER_H
#define SIMPLE_SOCKET_TEST_HELPER_H

#include <utils/thread/sync_queue.h>
#include <utils/common/systemerror.h>


// Template multitype socket tests to ensure that every common_ut run checks
// TCP and UDT basic functionality

template< typename ServerSocket, typename ClientSocket>
static void socketSimpleSync(
    const SocketAddress& serverAddress,
    const QByteArray& testMessage,
    int clientCount)
{
    std::thread serverThread([&serverAddress, &testMessage, clientCount]()
    {
        auto server = std::make_unique< ServerSocket >();
        ASSERT_TRUE(server->setNonBlockingMode(false));
        ASSERT_TRUE(server->setReuseAddrFlag(true));
        ASSERT_TRUE(server->bind(serverAddress)) << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(server->listen(clientCount)) << SystemError::getLastOSErrorText().toStdString();

        for (int i = clientCount; i > 0; --i)
        {
            static const int BUF_SIZE = 128;

            QByteArray buffer(BUF_SIZE, char(0));
            std::unique_ptr< AbstractStreamSocket > client(server->accept());
            ASSERT_TRUE(client->setNonBlockingMode(false));

            int bufDataSize = 0;
            for (;;)
            {
                const auto bytesRead = client->recv(
                    buffer.data() + bufDataSize,
                    buffer.size() - bufDataSize);
                ASSERT_NE(-1, bytesRead);
                if (bytesRead == 0)
                    break;  //connection closed
                bufDataSize += bytesRead;
            }

            EXPECT_STREQ(testMessage.data(), buffer.data());
        }
    });

    // give the server some time to start
    std::this_thread::sleep_for(std::chrono::microseconds(500));

    std::thread clientThread([&serverAddress, &testMessage, clientCount]()
    {
        for (int i = clientCount; i > 0; --i)
        {
            auto client = std::make_unique< ClientSocket >(false);
            EXPECT_TRUE(client->connect(serverAddress, 500));
            EXPECT_EQ(client->send(testMessage.data(), testMessage.size() + 1),
                testMessage.size() + 1);
        }
    });

    serverThread.join();
    clientThread.join();
}


template< typename ServerSocket, typename ClientSocket>
static void socketSimpleAsync(
    const SocketAddress& serverAddress,
    const QByteArray& testMessage,
    int clientCount)
{
    nx::SyncQueue< SystemError::ErrorCode > serverResults;
    nx::SyncQueue< SystemError::ErrorCode > clientResults;

    auto server = std::make_unique< ServerSocket >();
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

            client->terminateAsyncIO(true);
            server->acceptAsync(accept);
            serverResults.push(code);
        });
    };

    server->acceptAsync(accept);

    auto testClient = std::make_unique< ClientSocket >(false);
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

    testClient->terminateAsyncIO(true);
    server->terminateAsyncIO(true);
}

#endif  //SIMPLE_SOCKET_TEST_HELPER_H
