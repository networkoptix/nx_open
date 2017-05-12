#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/thread/cf/wrappers.h>

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

        clientSocket2 = SocketFactory::createStreamSocket();
        clientSocket2->setNonBlockingMode(true);

        m_acceptor->acceptAsync(
            [this](SystemError::ErrorCode ecode, std::unique_ptr<AbstractStreamSocket> clientSocket)
            {
                ASSERT_EQ(SystemError::noError, ecode);
                ASSERT_TRUE(clientSocket->setNonBlockingMode(true));

                m_acceptor.reset();
                clientSocket1 = std::move(clientSocket);
                clientSocket1->setNonBlockingMode(true);

                clientWebSocket.reset(
                    new nx::network::WebSocket(
                        std::move(clientSocket1),
                        clientSendMode,
                        clientReceiveMode));

                serverWebSocket.reset(
                    new nx::network::WebSocket(
                        std::move(clientSocket2),
                        serverSendMode,
                        serverReceiveMode));

                //clientWebSocket->bindToAioThread(serverWebSocket->getAioThread());
                clientWebSocket->setPingTimeout(std::chrono::milliseconds(kPingTimeout));

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
                if (ecode != SystemError::noError)
                    return;
                if (doneCount == kIterations)
                    return;
                clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
            };

        clientReadCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
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
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
                if (ecode != SystemError::noError)
                    return;
                serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
            };

        serverReadCb =
            [this](SystemError::ErrorCode ecode, size_t transferred)
            {
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
    std::unique_ptr<AbstractStreamSocket> clientSocket1;
    std::unique_ptr<AbstractStreamSocket> clientSocket2;

    nx::network::WebSocketPtr clientWebSocket;
    nx::network::WebSocketPtr serverWebSocket;

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
    givenTCPConnectionEstablished();

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
    givenTCPConnectionEstablished();

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
    givenTCPConnectionEstablished();

    startFuture.wait();

    clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
    clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

    readyFuture.wait();
}


cf::future<cf::unit> websocketTestReader(
    nx::network::WebSocketPtr& socket,
    SendMode sendMode,
    ReceiveMode receiveMode,
    int iterations)
{
    struct State
    {
        int count;
        int total;
        nx::Buffer buffer;
        nx::network::WebSocketPtr& webSocket;
        State(nx::network::WebSocketPtr& socket, SendMode sendMode, ReceiveMode receiveMode, int total):
            count (0),
            total(total),
            webSocket(socket)
        {}
        ~State(){}
    };
    auto state = std::make_shared<State>(socket, sendMode, receiveMode, iterations);
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
    nx::network::WebSocketPtr& socket,
    SendMode sendMode,
    ReceiveMode receiveMode,
    const nx::Buffer& sendBuffer,
    int iterations)
{
    struct State
    {
        int count;
        int total;
        nx::Buffer buffer;
        nx::network::WebSocketPtr& webSocket;
        State(nx::network::WebSocketPtr& socket, SendMode sendMode, ReceiveMode receiveMode, const nx::Buffer& buffer, int total):
            count (0),
            total(total),
            buffer(buffer),
            webSocket(socket)
        {}
    };
    auto state = std::make_shared<State>(socket, sendMode, receiveMode, sendBuffer, iterations);
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
    auto readFuture = websocketTestReader(
        serverWebSocket,
        SendMode::singleMessage,
        ReceiveMode::message,
        10);
    auto writeFuture = websocketTestWriter(
        clientWebSocket,
        SendMode::singleMessage,
        ReceiveMode::message,
        clientSendBuf,
        10);
    cf::when_all(readFuture, writeFuture).wait();
}

TEST_F(WebSocket, PingPong)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenPingTimeout(std::chrono::milliseconds(10));
    givenTCPConnectionEstablished();
    startFuture.wait();
    int pongCount = 0;
    QObject::connect(clientWebSocket.get(), &nx::network::WebSocket::pongReceived, [&pongCount]() { ++pongCount; });
    auto readFuture = websocketTestReader(
        serverWebSocket,
        SendMode::singleMessage,
        ReceiveMode::message,
        100);
    auto writeFuture = websocketTestWriter(
        clientWebSocket,
        SendMode::singleMessage,
        ReceiveMode::message,
        clientSendBuf,
        100);
    cf::when_all(readFuture, writeFuture).wait();
    ASSERT_GT(pongCount, 10);
}

TEST_F(WebSocket, Close)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientManyMessagesWithServerRespondingCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenTCPConnectionEstablished();

    startFuture.wait();
    bool clientClosed = false;
    bool serverClosed = false;

    QObject::connect(
        clientWebSocket.get(),
        &nx::network::WebSocket::connectionClosed,
        [&clientClosed, this]() { clientClosed = true; });
    QObject::connect(
        serverWebSocket.get(),
        &nx::network::WebSocket::connectionClosed,
        [&serverClosed, this]()
    {
        readyPromise.set_value();
        serverClosed = true;
    });
    clientWebSocket->sendAsync(clientSendBuf, clientSendCb);
    clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
    clientWebSocket->sendCloseAsync();

    readyFuture.wait();

    ASSERT_TRUE(clientClosed);
    ASSERT_TRUE(serverClosed);
}

}

