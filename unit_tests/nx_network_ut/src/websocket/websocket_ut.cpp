#include <future>
#include <gtest/gtest.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/socket_factory.h>

namespace test {

class Websocket : public ::testing::Test
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

                clientWebsocket.reset(
                    new nx::network::Websocket(
                        std::move(clientSocket1),
                        nx::Buffer()));

                serverWebsocket.reset(
                    new nx::network::Websocket(
                        std::move(clientSocket2),
                        nx::Buffer()));

                startPromise.set_value();
            });

        clientSocket2 = SocketFactory::createStreamSocket();
        ASSERT_TRUE(clientSocket2->connect(m_acceptor->getLocalAddress()));

        readyFuture = readyPromise.get_future();
        startFuture = startPromise.get_future();
    }

    virtual void TearDown() override
    {
        //clientSocket1->pleaseStopSync();
        //clientSocket2->pleaseStopSync();
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

    std::unique_ptr<nx::network::Websocket> clientWebsocket;
    std::unique_ptr<nx::network::Websocket> serverWebsocket;

    std::promise<void> startPromise;
    std::future<void> startFuture;

    std::promise<void> readyPromise;
    std::future<void> readyFuture;

    nx::Buffer readBuffer;
};

TEST_F(Websocket, SingleMessage_singleTransfer)
{
    startFuture.wait();

    nx::Buffer sendBuf;
    prepareTestData(&sendBuf, 1024 * 1024 * 10);

    clientWebsocket->sendAsync(
        sendBuf,
        [this](SystemError::ErrorCode ecode, size_t transferred)
        {
            ASSERT_EQ(ecode, SystemError::noError);
        });

    serverWebsocket->readSomeAsync(
        &readBuffer,
        [this](SystemError::ErrorCode ecode, size_t transferred)
        {
            auto i = transferred;
            readyPromise.set_value();
        });

    readyFuture.wait();
    ASSERT_EQ(readBuffer, sendBuf);
}

TEST_F(Websocket, SingleMessage_singleTransfer)
{
}

}

