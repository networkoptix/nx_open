#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/socket_factory.h>

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
                        nx::Buffer()));

                serverWebSocket.reset(
                    new nx::network::WebSocket(
                        std::move(clientSocket2),
                        nx::Buffer()));

                clientWebSocket->bindToAioThread(serverWebSocket->getAioThread());

                startPromise.set_value();
            });

        clientSocket2 = SocketFactory::createStreamSocket();
        clientSocket2->setNonBlockingMode(true);
        ASSERT_TRUE(clientSocket2->connect(m_acceptor->getLocalAddress()));

        readyFuture = readyPromise.get_future();
        startFuture = startPromise.get_future();
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

    std::unique_ptr<AbstractStreamServerSocket> m_acceptor;
    std::unique_ptr<AbstractStreamSocket> clientSocket1;
    std::unique_ptr<AbstractStreamSocket> clientSocket2;

    std::unique_ptr<nx::network::WebSocket> clientWebSocket;
    std::unique_ptr<nx::network::WebSocket> serverWebSocket;

    std::promise<void> startPromise;
    std::future<void> startFuture;

    std::promise<void> readyPromise;
    std::future<void> readyFuture;

    nx::Buffer readBuffer;
};

TEST_F(WebSocket, SingleMessage_singleTransfer)
{
    startFuture.wait();

    nx::Buffer sendBuf;
    prepareTestData(&sendBuf, 1024 * 1024 * 10);

    clientWebSocket->sendAsync(
        sendBuf,
        [this](SystemError::ErrorCode ecode, size_t transferred)
        {
            ASSERT_EQ(ecode, SystemError::noError);
        });

    serverWebSocket->readSomeAsync(
        &readBuffer,
        [this](SystemError::ErrorCode ecode, size_t transferred)
        {
            readyPromise.set_value();
        });

    readyFuture.wait();
    ASSERT_EQ(readBuffer, sendBuf);
}

TEST_F(WebSocket, MultipleMessages_oneWay)
{
    startFuture.wait();

    int kIterations = 10;
    int doneCount = 0;
    nx::Buffer sendBuf;
    prepareTestData(&sendBuf, 1024 * 1024 * 10);
    readBuffer.reserve(1024 * 1024 * 11);

    std::function<void(SystemError::ErrorCode, size_t)> sendCb =
        [this, &doneCount, &kIterations, &sendBuf, &sendCb](SystemError::ErrorCode ecode, size_t transferred)
        {
            ASSERT_EQ(ecode, SystemError::noError);
            if (doneCount == kIterations)
                return;
            clientWebSocket->sendAsync(sendBuf, sendCb);
        };

    std::function<void(SystemError::ErrorCode, size_t)> readCb =
        [this, &doneCount, &kIterations, &sendBuf, &readCb](SystemError::ErrorCode ecode, size_t transferred)
        {
            ASSERT_EQ(ecode, SystemError::noError);
            doneCount++;
            if (doneCount == kIterations)
            {
                readyPromise.set_value();
                return;
            }
            ASSERT_EQ(readBuffer, sendBuf);
            readBuffer.clear();
            readBuffer.reserve(1024 * 1024 * 11);
            auto capacity = readBuffer.capacity();
            auto size = readBuffer.size();
            serverWebSocket->readSomeAsync(&readBuffer, readCb);
        };

    clientWebSocket->sendAsync(sendBuf, sendCb);
    serverWebSocket->readSomeAsync(&readBuffer, readCb);

    readyFuture.wait();
}

TEST_F(WebSocket, MultipleMessages_twoWay)
{
    startFuture.wait();

    int kIterations = 10;
    int doneCount = 0;

    nx::Buffer clientSendBuf;
    prepareTestData(&clientSendBuf, 1024 * 1024 * 10);
    nx::Buffer clientReadBuf;

    nx::Buffer serverSendBuf;
    prepareTestData(&serverSendBuf, 1024 * 1024 * 3);
    nx::Buffer serverReadBuf;

    std::function<void(SystemError::ErrorCode, size_t)> clientSendCb;
    std::function<void(SystemError::ErrorCode, size_t)> clientReadCb;
    std::function<void(SystemError::ErrorCode, size_t)> serverSendCb;
    std::function<void(SystemError::ErrorCode, size_t)> serverReadCb;

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (doneCount == kIterations || ecode != SystemError::noError)
                return;
            clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
        };

    clientReadCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            doneCount++;
            if (doneCount == kIterations || ecode != SystemError::noError)
            {
                //clientWebSocket->pleaseStop([]() {});
                //serverWebSocket->pleaseStop([]() {});
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                readyPromise.set_value();
                return;
            }

            ASSERT_EQ(clientReadBuf, serverSendBuf);
            clientReadBuf.clear();
            clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
        };

    serverSendCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (doneCount == kIterations || ecode != SystemError::noError)
                return;
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };

    serverReadCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (doneCount == kIterations || ecode != SystemError::noError)
                return;

            ASSERT_EQ(serverReadBuf, clientSendBuf);
            serverReadBuf.clear();
            serverWebSocket->sendAsync(serverSendBuf, serverSendCb);
        };

    clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

    readyFuture.wait();
}

}

