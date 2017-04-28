#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/socket_factory.h>

using namespace nx::network::websocket;

namespace test {

class WebSocket : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_acceptor = SocketFactory::createStreamServerSocket();

        ASSERT_TRUE(m_acceptor->setNonBlockingMode(true));
        ASSERT_TRUE(m_acceptor->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_acceptor->listen());
        m_acceptor->acceptAsync(
            [this](SystemError::ErrorCode ecode, AbstractStreamSocket* clientSocket)
            {
                ASSERT_EQ(SystemError::noError, ecode);
                ASSERT_TRUE(clientSocket->setNonBlockingMode(true));

                m_acceptor.reset();
                clientSocket1.reset(clientSocket);

                clientWebSocket.reset(
                    new nx::network::WebSocket(
                        std::move(clientSocket1),
                        nx::Buffer(),
                        clientSendMode,
                        clientReceiveMode));

                serverWebSocket.reset(
                    new nx::network::WebSocket(
                        std::move(clientSocket2),
                        nx::Buffer(),
                        serverSendMode,
                        serverReceiveMode));

                clientWebSocket->bindToAioThread(serverWebSocket->getAioThread());

                startPromise.set_value();
            });

        clientSocket2 = SocketFactory::createStreamSocket();
        clientSocket2->setNonBlockingMode(true);

        startFuture = startPromise.get_future();
        readyFuture = readyPromise.get_future();

    }

    void givenSocketAreConnected()
    {
        ASSERT_TRUE(clientSocket2->connect(m_acceptor->getLocalAddress()));
    }

    void givenClientTestDataPrepared(int size)
    {
        prepareTestData(&clientSendBuf, size);
    }

    void givenServerTestDataPrepared(int size)
    {
        prepareTestData(&serverSendBuf, size);
    }

    void givenClientModes(SendMode sendMode, ReceiveMode receiveMode)
    {
        clientSendMode = sendMode;
        clientReceiveMode = receiveMode;
    }

    void givenServerModes(SendMode sendMode, ReceiveMode receiveMode)
    {
        serverSendMode = sendMode;
        serverReceiveMode = receiveMode;
    }

    void givenClientServerExchangeMessagesCallbacks()
    {
        clientSendCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                ASSERT_EQ(ecode, SystemError::noError);
                if (doneCount == kIterations)
                    return;
                clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
            };

        clientReadCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                ASSERT_EQ(ecode, SystemError::noError);
                doneCount++;
                if (doneCount == kIterations)
                {
                    readyPromise.set_value();
                    return;
                }

                ASSERT_EQ(clientReadBuf, serverSendBuf);
                clientReadBuf.clear();
                clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
            };

        serverSendCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                ASSERT_EQ(ecode, SystemError::noError);
                if (doneCount == kIterations)
                    return;
                serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
            };

        serverReadCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                if (doneCount == kIterations)
                    return;

                ASSERT_EQ(serverReadBuf, clientSendBuf);
                serverReadBuf.clear();
                serverWebSocket->sendAsync(serverSendBuf, serverSendCb);
            };
    }

    void givenClientManyMessagesWithServerRespondingCallbacks()
    {
        clientSendCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                ASSERT_EQ(ecode, SystemError::noError);
                if (doneCount == kIterations)
                    return;
                clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
            };

        clientReadCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                ASSERT_EQ(clientReadBuf, serverSendBuf);
                clientReadBuf.clear();

                doneCount++;
                if (doneCount == kIterations)
                {
                    readyPromise.set_value();
                    return;
                }

                clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
            };

        serverSendCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                ASSERT_EQ(ecode, SystemError::noError);
                serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
            };

        serverReadCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                ASSERT_EQ(serverReadBuf, clientSendBuf);
                serverReadBuf.clear();
                serverWebSocket->sendAsync(serverSendBuf, serverSendCb);
            };
    }

    virtual void TearDown() override
    {
        clientWebSocket->pleaseStopSync();
        serverWebSocket->pleaseStopSync();
    }

    void prepareTestData(nx::Buffer* payload, int size)
    {
        static const char* const kPattern = "hello";
        static const int kPatternSize = (int)std::strlen(kPattern);

        payload->resize((size_t)size);
        char* pdata = payload->data();

        while (size > 0)
        {
            int copySize = std::min(kPatternSize, size);
            memcpy(pdata, kPattern, copySize);
            size -= copySize;
            pdata += copySize;
        }
    }

    int kIterations = 10;
    int doneCount = 0;

    nx::Buffer clientSendBuf;
    nx::Buffer clientReadBuf;
    SendMode clientSendMode;
    ReceiveMode clientReceiveMode;

    nx::Buffer serverSendBuf;
    nx::Buffer serverReadBuf;
    SendMode serverSendMode;
    ReceiveMode serverReceiveMode;

    std::function<void(SystemError::ErrorCode, size_t)> clientSendCb;
    std::function<void(SystemError::ErrorCode, size_t)> clientReadCb;
    std::function<void(SystemError::ErrorCode, size_t)> serverSendCb;
    std::function<void(SystemError::ErrorCode, size_t)> serverReadCb;

    std::unique_ptr<AbstractStreamServerSocket> m_acceptor;
    std::unique_ptr<AbstractStreamSocket> clientSocket1;
    std::unique_ptr<AbstractStreamSocket> clientSocket2;

    std::unique_ptr<nx::network::WebSocket> clientWebSocket;
    std::unique_ptr<nx::network::WebSocket> serverWebSocket;

    std::promise<void> startPromise;
    std::future<void> startFuture;

    std::promise<void> readyPromise;
    std::future<void> readyFuture;
};

TEST_F(WebSocket, MultipleMessages_twoWay)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientServerExchangeMessagesCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenSocketAreConnected();

    startFuture.wait();

    clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

    readyFuture.wait();
}

TEST_F(WebSocket, MultipleMessages_ReceiveModeFrame_twoWay)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientServerExchangeMessagesCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::frame);
    givenServerModes(SendMode::singleMessage, ReceiveMode::frame);
    givenSocketAreConnected();

    startFuture.wait();

    clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

    readyFuture.wait();
}

TEST_F(WebSocket, MultipleMessagesFromClient_ServerResponds)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientManyMessagesWithServerRespondingCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenSocketAreConnected();

    startFuture.wait();

    clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
    clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

    readyFuture.wait();
}

}

