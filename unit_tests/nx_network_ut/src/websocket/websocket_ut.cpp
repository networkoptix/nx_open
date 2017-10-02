#include <functional>
#include <future>
#include <atomic>
#include <gtest/gtest.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/socket_delegate.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/thread/cf/wrappers.h>

using namespace nx::network::websocket;

namespace test {

class TestStreamSocketDelegate : public nx::network::StreamSocketDelegate
{
public:
    using nx::network::StreamSocketDelegate::StreamSocketDelegate;
    virtual ~TestStreamSocketDelegate() override { delete m_target; }

    virtual void readSomeAsync(nx::Buffer* const buf, HandlerType handler) override
    {
        if (m_terminated)
        {
            m_target->post([handler = std::move(handler)]() { handler(SystemError::connectionAbort, 0); });
            return;
        }
        if (m_zeroRead)
        {
            m_target->post([handler = std::move(handler)]() { handler(SystemError::noError, 0); });
            return;
        }
        m_target->readSomeAsync(buf, handler);
    }

    virtual void sendAsync(const nx::Buffer& buf, HandlerType handler) override
    {
        if (m_terminated)
        {
            m_target->post([handler = std::move(handler)]() { handler(SystemError::connectionAbort, 0); });
            return;
        }
        m_target->sendAsync(buf, handler);
    }

    void terminate() { m_terminated = true; }
    void setZeroRead() { m_zeroRead = true; }

private:
    std::atomic<bool> m_terminated{false};
    std::atomic<bool> m_zeroRead{ false };
};

class TestWebSocket : public nx::network::WebSocket
{
public:
    using WebSocket::WebSocket;
    int pingCount() { return m_pingsReceived; }
    int pongCount() { return m_pongsReceived; }
    std::chrono::milliseconds pingTimeout() const { return m_pingTimeout; }
    TestStreamSocketDelegate* socket() { return dynamic_cast<TestStreamSocketDelegate*>(WebSocket::socket()); }
};

class WebSocket : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_acceptor = SocketFactory::createStreamServerSocket();

        ASSERT_TRUE(m_acceptor->setNonBlockingMode(true));
        ASSERT_TRUE(m_acceptor->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_acceptor->listen());

        clientSocket2 = std::unique_ptr<TestStreamSocketDelegate>(
            new TestStreamSocketDelegate(SocketFactory::createStreamSocket().release()));
        clientSocket2->setNonBlockingMode(true);

        m_acceptor->acceptAsync(
            [this](SystemError::ErrorCode ecode, std::unique_ptr<AbstractStreamSocket> clientSocket)
            {
                ASSERT_EQ(SystemError::noError, ecode);
                ASSERT_TRUE(clientSocket->setNonBlockingMode(true));

                m_acceptor.reset();
                clientSocket1 = std::unique_ptr<TestStreamSocketDelegate>(
                    new TestStreamSocketDelegate(clientSocket.release()));
                clientSocket1->setNonBlockingMode(true);

                clientWebSocket.reset(
                    new TestWebSocket(
                        std::move(clientSocket1),
                        clientSendMode,
                        clientReceiveMode));

                serverWebSocket.reset(
                    new TestWebSocket(
                        std::move(clientSocket2),
                        serverSendMode,
                        serverReceiveMode,
                        serverRole));

                startPromise.set_value();
            });

        startFuture = startPromise.get_future();
        readyFuture = readyPromise.get_future();

    }

    void givenPingTimeout(std::chrono::milliseconds timeout)
    {
        kPingTimeout = timeout;
    }

    void givenTCPConnectionEstablished()
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
            [this](SystemError::ErrorCode ecode, size_t)
            {
                ASSERT_EQ(ecode, SystemError::noError);
                if (doneCount == kIterations)
                    return;
                clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
            };

        clientReadCb =
            [this](SystemError::ErrorCode ecode, size_t)
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
            [this](SystemError::ErrorCode ecode, size_t)
            {
                ASSERT_EQ(ecode, SystemError::noError);
                if (doneCount == kIterations)
                    return;
                serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
            };

        serverReadCb =
            [this](SystemError::ErrorCode, size_t)
            {
                if (doneCount == kIterations)
                    return;

                ASSERT_EQ(serverReadBuf, clientSendBuf);
                serverReadBuf.clear();
                serverWebSocket->sendAsync(serverSendBuf, serverSendCb);
            };
    }

    void whenSocketsBoundToOneThreadAndReadWriteScheduled()
    {
        whenSocketsBoundToOneThread();
        clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
        serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
    }

    void whenSocketsBoundToOneThread()
    {
        clientWebSocket->cancelIOSync(nx::network::aio::EventType::etNone);
        clientWebSocket->bindToAioThread(serverWebSocket->getAioThread());
    }

    void givenClientManyMessagesWithServerRespondingCallbacks()
    {
        clientSendCb =
            [this](SystemError::ErrorCode ecode, size_t)
            {
            if (ecode == SystemError::connectionAbort)
            {
                try { readyPromise.set_value(); } catch (...) {}
            }
                if (ecode != SystemError::noError)
                    return;
                if (doneCount == kIterations)
                    return;
                clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
            };

        clientReadCb =
            [this](SystemError::ErrorCode ecode, size_t)
            {
                if (ecode == SystemError::connectionAbort)
                {
                    try { readyPromise.set_value(); } catch (...) {}
                }
               if (ecode != SystemError::noError)
                    return;
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
            [this](SystemError::ErrorCode ecode, size_t)
            {
                if (ecode == SystemError::connectionAbort)
                {
                    try { readyPromise.set_value(); } catch (...) {}
                }
               if (ecode != SystemError::noError)
                    return;
                serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
            };

        serverReadCb =
            [this](SystemError::ErrorCode ecode, size_t)
            {
                if (ecode == SystemError::connectionAbort)
                {
                    try { readyPromise.set_value(); } catch (...) {}
                }
                if (ecode != SystemError::noError)
                    return;
                serverReadBuf.clear();
                serverWebSocket->sendAsync(serverSendBuf, serverSendCb);
            };
    }

    virtual void TearDown() override
    {
        if (clientWebSocket)
            clientWebSocket->pleaseStopSync();
        if (serverWebSocket)
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
    std::chrono::milliseconds kPingTimeout{100000};

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
    std::unique_ptr<TestStreamSocketDelegate> clientSocket1;
    std::unique_ptr<TestStreamSocketDelegate> clientSocket2;

    std::unique_ptr<TestWebSocket> clientWebSocket;
    std::unique_ptr<TestWebSocket> serverWebSocket;

    std::promise<void> startPromise;
    std::future<void> startFuture;

    std::promise<void> readyPromise;
    std::future<void> readyFuture;

    Role serverRole = Role::undefined;
};

TEST_F(WebSocket, MultipleMessages_twoWay)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientServerExchangeMessagesCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenTCPConnectionEstablished();

    startFuture.wait();
    whenSocketsBoundToOneThreadAndReadWriteScheduled();

    readyFuture.wait();
}

TEST_F(WebSocket, MultipleMessages_ReceiveModeFrame_twoWay)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientServerExchangeMessagesCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::frame);
    givenServerModes(SendMode::singleMessage, ReceiveMode::frame);
    givenTCPConnectionEstablished();

    startFuture.wait();
    clientWebSocket->cancelIOSync(nx::network::aio::EventType::etNone);
    clientWebSocket->bindToAioThread(serverWebSocket->getAioThread());

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
    givenTCPConnectionEstablished();

    startFuture.wait();

    whenSocketsBoundToOneThreadAndReadWriteScheduled();
    clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);

    readyFuture.wait();
}


cf::future<cf::unit> websocketTestReader(std::unique_ptr<TestWebSocket>& socket, int iterations)
{
    struct State
    {
        int count;
        int total;
        nx::Buffer buffer;
        std::unique_ptr<TestWebSocket>& webSocket;
        State(std::unique_ptr<TestWebSocket>& socket, int total):
            count (0),
            total(total),
            webSocket(socket)
        {}
        ~State(){}
    };
    auto state = std::make_shared<State>(socket, iterations);
    return cf::doWhile(
        *state->webSocket,
        [state]() mutable
        {
            if (state->count++ >= state->total)
                return cf::make_ready_future(false);
            return cf::readAsync(state->webSocket.get(), &state->buffer).then(
                [state](cf::future<std::pair<SystemError::ErrorCode, size_t>> readFuture)
                {
                    auto resultPair = readFuture.get();
                    if (resultPair.first != SystemError::noError)
                        return cf::make_ready_future(false);
                    return cf::sendAsync(state->webSocket.get(), state->buffer).then(
                        [state](cf::future<std::pair<SystemError::ErrorCode, size_t>> sendFuture)
                        {
                            auto resultPair = sendFuture.get();
                            if (resultPair.first != SystemError::noError)
                                return false;
                            state->buffer.clear();
                            return true;
                    });
                });
        });
}

cf::future<cf::unit> websocketTestWriter(
    std::unique_ptr<TestWebSocket>& socket,
    const nx::Buffer& sendBuffer,
    int iterations)
{
    struct State
    {
        int count;
        int total;
        nx::Buffer buffer;
        std::unique_ptr<TestWebSocket>& webSocket;
        State(std::unique_ptr<TestWebSocket>& socket, const nx::Buffer& buffer, int total):
            count (0),
            total(total),
            buffer(buffer),
            webSocket(socket)
        {}
    };
    auto state = std::make_shared<State>(socket, sendBuffer, iterations);
    return cf::doWhile(
        *state->webSocket,
        [state]() mutable
        {
            if (state->count++ >= state->total)
                return cf::make_ready_future(false);
            return cf::sendAsync(state->webSocket.get(), state->buffer).then(
                [state](cf::future<std::pair<SystemError::ErrorCode, size_t>> sendFuture)
                {
                    auto resultPair = sendFuture.get();
                    if (resultPair.first != SystemError::noError)
                        return cf::make_ready_future(false);
                    state->buffer.clear();
                    return cf::readAsync(state->webSocket.get(), &state->buffer).then(
                        [state](cf::future<std::pair<SystemError::ErrorCode, size_t>> readFuture)
                        {
                            auto resultPair = readFuture.get();
                            if (resultPair.first != SystemError::noError)
                                return false;
                            return true;
                        });
                });
        });
}

TEST_F(WebSocket, Wrappers)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenTCPConnectionEstablished();
    startFuture.wait();
    clientWebSocket->cancelIOSync(nx::network::aio::EventType::etNone);
    clientWebSocket->bindToAioThread(serverWebSocket->getAioThread());
    auto readFuture = websocketTestReader(serverWebSocket, 10);
    auto writeFuture = websocketTestWriter(clientWebSocket, clientSendBuf, 10);
    cf::when_all(readFuture, writeFuture).wait();
}


class WebSocket_PingPong : public WebSocket
{
protected:
    virtual void SetUp() override
    {
        WebSocket::SetUp();
        givenClientModes(SendMode::singleMessage, ReceiveMode::message);
        givenServerModes(SendMode::singleMessage, ReceiveMode::message);
        givenClientTestDataPrepared(1024 * 1024 * 3);
        givenTCPConnectionEstablished();

        clientSendCb =
            [&](SystemError::ErrorCode ecode, size_t)
        {
            if (ecode != SystemError::noError)
            {
                //std::cout << "Client send error: " << ecode << std::endl;
                pongCount = clientWebSocket->pongCount();
                clientWebSocket.reset();
                processError(ecode);
                return;
            }

            ++sentMessageCount;
            //std::cout << "Message count: " << sentMessageCount << std::endl;

            if (sentMessageCount >= kTotalMessageCount)
            {
                pongCount = clientWebSocket->pongCount();

                clientWebSocket.reset();
                try { readyPromise.set_value(); } catch (...) {}
                return;
            }

            clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
        };

        clientReadCb =
            [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (ecode != SystemError::noError || transferred == 0)
            {
                //std::cout << "Client read error: " << ecode << std::endl;
                clientWebSocket.reset();
                processError(ecode);

                return;
            }

            clientReadBuf.clear();
            clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
        };

        serverSendCb =
            [&](SystemError::ErrorCode ecode, size_t)
        {

            if (ecode != SystemError::noError)
            {
                //std::cout << "server send error: " << ecode << std::endl;
                serverWebSocket.reset();
                processError(ecode);
                return;
            }

            //std::cout << "server send cb. Success." << std::endl;
            if (!sendQueue.empty())
            {
                auto bufToSend = sendQueue.front();
                sendQueue.pop();
                serverWebSocket->sendAsync(bufToSend, serverSendCb);
            }
        };

        serverReadCb =
            [&](SystemError::ErrorCode ecode, size_t transferred)
        {

            if (ecode != SystemError::noError || transferred == 0)
            {
                //std::cout << "server read error: " << ecode << std::endl;
                serverWebSocket.reset();
                //std::cout << "server destroyed" << std::endl;
                processError(ecode);

                return;
            }

            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
            if (isServerResponds)
            {
                if (sendQueue.empty())
                    serverWebSocket->sendAsync(serverReadBuf, serverSendCb);
                else
                    sendQueue.push(serverReadBuf);
            }
            serverReadBuf.clear();
        };

        startFuture.wait();
        clientWebSocket->cancelIOSync(nx::network::aio::EventType::etNone);
        clientWebSocket->bindToAioThread(serverWebSocket->getAioThread());
        clientWebSocket->setAliveTimeout(kAliveTimeout);
    }

    void processError(SystemError::ErrorCode ecode)
    {
        if (ecode == SystemError::timedOut)
            isTimeoutError = true;

        try { readyPromise.set_value(); } catch (...) {}
    }

    void start()
    {
        clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
        clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
        serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

        if (beforeWaitAction)
            beforeWaitAction();

        readyFuture.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }


    std::function<void()> beforeWaitAction;

    bool isServerResponds = true;
    bool isTimeoutError = false;
    int sentMessageCount = 0;
    const int kTotalMessageCount = 100;
    std::queue<nx::Buffer> sendQueue;
    const std::chrono::milliseconds kAliveTimeout = std::chrono::milliseconds(3000);
    int pongCount = 0;
};

TEST_F(WebSocket_PingPong, PingPong_noPingsBecauseOfData)
{
    start();
    ASSERT_FALSE(isTimeoutError);
    ASSERT_EQ(pongCount, 0);
}

TEST_F(WebSocket_PingPong, PingPong_pingsBecauseOfNoData)
{
    isServerResponds = false;
    start();

    ASSERT_FALSE(isTimeoutError);
    ASSERT_GT(pongCount, 0);
}


TEST_F(WebSocket_PingPong, Close)
{
    beforeWaitAction =
        [this]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        clientWebSocket->sendCloseAsync();
    };

    start();

    ASSERT_FALSE(clientWebSocket);
    ASSERT_FALSE(serverWebSocket);
}

TEST_F(WebSocket, SendMultiFrame_ReceiveSingleMessage)
{
    givenClientModes(SendMode::multiFrameMessage, ReceiveMode::frame);
    givenServerModes(SendMode::multiFrameMessage, ReceiveMode::message);
    givenTCPConnectionEstablished();

    int frameCount = 0;
    int sentMessageCount = 0;
    int receivedMessageCount = 0;
    const int kMessageFrameCount = 100;
    const int kTotalMessageCount = 20;
    const nx::Buffer kFrameBuffer("hello");

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t)
        {
            ASSERT_EQ(ecode, SystemError::noError);
            frameCount++;
            if (frameCount == kMessageFrameCount - 1)
                clientWebSocket->setIsLastFrame();
            else if (frameCount == kMessageFrameCount)
            {
                frameCount = 0;
                sentMessageCount++;
            }
            if (sentMessageCount >= kTotalMessageCount)
            {
                readyPromise.set_value();
                return;
            }
            clientWebSocket->sendAsync(kFrameBuffer, clientSendCb);
        };

    serverReadCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (ecode != SystemError::noError || transferred == 0)
                return;
            ASSERT_EQ(serverReadBuf.size(), kFrameBuffer.size()*kMessageFrameCount);
            receivedMessageCount++;
            serverReadBuf.clear();
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };
    startFuture.wait();
    whenSocketsBoundToOneThread();

    clientWebSocket->sendAsync(kFrameBuffer, clientSendCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

    readyFuture.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(receivedMessageCount, kTotalMessageCount);
}

TEST_F(WebSocket, SendMultiFrame_ReceiveFrame)
{
    givenClientModes(SendMode::multiFrameMessage, ReceiveMode::frame);
    givenServerModes(SendMode::multiFrameMessage, ReceiveMode::frame);
    givenTCPConnectionEstablished();

    int frameCount = 0;
    int sentMessageCount = 0;
    int receivedFrameCount = 0;
    const int kMessageFrameCount = 100;
    const int kTotalMessageCount = 20;
    const nx::Buffer kFrameBuffer("hello");

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t)
        {
            ASSERT_EQ(ecode, SystemError::noError);
            frameCount++;
            if (frameCount == kMessageFrameCount - 1)
                clientWebSocket->setIsLastFrame();
            else if (frameCount == kMessageFrameCount)
            {
                frameCount = 0;
                sentMessageCount++;
            }
            if (sentMessageCount >= kTotalMessageCount)
            {
                readyPromise.set_value();
                return;
            }
            clientWebSocket->sendAsync(kFrameBuffer, clientSendCb);
        };

    serverReadCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (ecode != SystemError::noError || transferred == 0)
                return;
            ASSERT_EQ(serverReadBuf.size(), kFrameBuffer.size());
            receivedFrameCount++;
            serverReadBuf.clear();
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };
    startFuture.wait();

    whenSocketsBoundToOneThread();
    clientWebSocket->sendAsync(kFrameBuffer, clientSendCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

    readyFuture.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(receivedFrameCount, kTotalMessageCount*kMessageFrameCount);
}

TEST_F(WebSocket, SendMultiFrame_ReceiveStream)
{
    givenClientModes(SendMode::multiFrameMessage, ReceiveMode::frame);
    givenServerModes(SendMode::multiFrameMessage, ReceiveMode::stream);
    givenClientTestDataPrepared(16384 + 17);
    givenTCPConnectionEstablished();

    int frameCount = 0;
    int sentMessageCount = 0;
    int receivedDataSize = 0;
    const int kMessageFrameCount = 100;
    const int kTotalMessageCount = 20;
    const int kReceiveAmount = kTotalMessageCount*kMessageFrameCount*clientSendBuf.size();

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t)
        {
            ASSERT_EQ(ecode, SystemError::noError);
            frameCount++;
            if (frameCount == kMessageFrameCount - 1)
                clientWebSocket->setIsLastFrame();
            else if (frameCount == kMessageFrameCount)
            {
                frameCount = 0;
                sentMessageCount++;
            }
            if (sentMessageCount >= kTotalMessageCount)
                return;
            clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
        };

    serverReadCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (ecode != SystemError::noError || transferred == 0)
                return;
            receivedDataSize += transferred;
            serverReadBuf.clear();

            if (receivedDataSize == kReceiveAmount)
            {
                readyPromise.set_value();
                return;
            }
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };
    startFuture.wait();
    whenSocketsBoundToOneThreadAndReadWriteScheduled();

    readyFuture.wait();
    ASSERT_TRUE(true);
}

TEST_F(WebSocket, SendMessage_ReceiveStream)
{
    givenClientModes(SendMode::singleMessage, ReceiveMode::frame);
    givenServerModes(SendMode::multiFrameMessage, ReceiveMode::stream);
    givenClientTestDataPrepared(1000 * 1000 * 10);
    givenTCPConnectionEstablished();

    int sentMessageCount = 0;
    int receivedDataSize = 0;
    const int kTotalMessageCount = 2;
    const int kReceiveAmount = kTotalMessageCount*clientSendBuf.size();

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t)
        {
            ASSERT_EQ(ecode, SystemError::noError);
            sentMessageCount++;

            if (sentMessageCount >= kTotalMessageCount)
                return;

            clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
        };

    serverReadCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (ecode != SystemError::noError || transferred == 0)
                return;
            receivedDataSize += transferred;
            serverReadBuf.clear();
            if (receivedDataSize == kReceiveAmount)
            {
                readyPromise.set_value();
                return;
            }
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };
    startFuture.wait();
    whenSocketsBoundToOneThreadAndReadWriteScheduled();
    readyFuture.wait();
    ASSERT_TRUE(true);
}

TEST_F(WebSocket_PingPong, UnexpectedClose_deleteFromCb_send)
{
    beforeWaitAction =
        [this]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        clientWebSocket->socket()->terminate();
    };

    start();

    ASSERT_FALSE(clientWebSocket);
    ASSERT_FALSE(serverWebSocket);
}

TEST_F(WebSocket_PingPong, UnexpectedClose_deleteFromCb_receive)
{
    beforeWaitAction =
        [this]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        serverWebSocket->socket()->terminate();
    };

    start();

    ASSERT_FALSE(clientWebSocket);
    ASSERT_FALSE(serverWebSocket);
}

TEST_F(WebSocket, UnexpectedClose_deleteFromCb_ParseError)
{
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenClientTestDataPrepared(1684*1024 + 17);
    serverRole = Role::server;
    givenTCPConnectionEstablished();

    int sentMessageCount = 0;
    const int kTotalMessageCount = 100;

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t)
        {
            if (ecode != SystemError::noError)
            {
                clientWebSocket.reset();
                try { readyPromise.set_value(); } catch (...) {}
                return;
            }
            sentMessageCount++;
            if (sentMessageCount >= kTotalMessageCount)
            {
                readyPromise.set_value();
                return;
            }
            clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
        };

    serverReadCb =
        [&](SystemError::ErrorCode ecode, size_t)
        {
            if (ecode != SystemError::noError)
            {
                serverWebSocket.reset();
                try { readyPromise.set_value(); } catch (...) {}
                return;
            }
            serverReadBuf.clear();
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };
    startFuture.wait();
    whenSocketsBoundToOneThreadAndReadWriteScheduled();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    readyFuture.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_TRUE(!serverWebSocket);
}

TEST_F(WebSocket, UnexpectedClose_ReadReturnedZero)
{
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenClientTestDataPrepared(1684 * 1024 + 17);
    givenTCPConnectionEstablished();

    int sentMessageCount = 0;
    const int kTotalMessageCount = 100;

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t)
    {
        if (ecode != SystemError::noError)
        {
            clientWebSocket.reset();
            try { readyPromise.set_value(); }
            catch (...) {}
            return;
        }
        sentMessageCount++;
        if (sentMessageCount >= kTotalMessageCount)
        {
            readyPromise.set_value();
            return;
        }
        clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
    };

    serverReadCb =
        [&](SystemError::ErrorCode, size_t bytesRead)
    {
        if (bytesRead == 0)
        {
            serverWebSocket.reset();
            try { readyPromise.set_value(); }
            catch (...) {}
            return;
        }
        serverReadBuf.clear();
        serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
    };
    startFuture.wait();
    whenSocketsBoundToOneThreadAndReadWriteScheduled();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    serverWebSocket->socket()->setZeroRead();

    readyFuture.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_TRUE(!serverWebSocket);
}

TEST_F(WebSocket_PingPong, calcPingTimeoutBySocketTimeout)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(clientWebSocket->pingTimeout(), kAliveTimeout / 4);
    unsigned recvTimeout;
    clientWebSocket->socket()->getRecvTimeout(&recvTimeout);
    ASSERT_EQ(recvTimeout, kAliveTimeout.count());
}

}

