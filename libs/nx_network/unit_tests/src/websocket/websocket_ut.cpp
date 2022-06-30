// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <functional>
#include <future>
#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/websocket/websocket.h>
#include <nx/network/socket_delegate.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/thread/cf/wrappers.h>

namespace nx::network::websocket::test {

class TestStreamSocketDelegate : public nx::network::StreamSocketDelegate
{
public:
    using nx::network::StreamSocketDelegate::StreamSocketDelegate;
    virtual ~TestStreamSocketDelegate() override { delete m_target; }

    virtual void readSomeAsync(nx::Buffer* const buf, IoCompletionHandler handler) override
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

        m_target->readSomeAsync(buf, std::move(handler));
    }

    virtual void sendAsync(const nx::Buffer* buf, IoCompletionHandler handler) override
    {
        if (m_terminated)
        {
            m_target->post([handler = std::move(handler)]() { handler(SystemError::connectionAbort, 0); });
            return;
        }

        m_target->sendAsync(buf, std::move(handler));
    }

    void terminate() { m_terminated = true; }
    void setZeroRead() { m_zeroRead = true; }

private:
    std::atomic<bool> m_terminated{false};
    std::atomic<bool> m_zeroRead{false};
};

class TestWebSocket : public nx::network::WebSocket
{
public:
    using WebSocket::WebSocket;
    TestStreamSocketDelegate* socket() { return dynamic_cast<TestStreamSocketDelegate*>(WebSocket::socket()); }
};

// ConnectedSocketsSupplier ------------------------------------------------------------------------
class ConnectedSocketsSupplier
{
public:
    ConnectedSocketsSupplier();
    ~ConnectedSocketsSupplier();

    bool connectSockets();
    std::unique_ptr<AbstractStreamSocket> clientSocket();
    std::unique_ptr<AbstractStreamSocket> serverSocket();
    std::string lastError();

private:
    std::unique_ptr<AbstractStreamServerSocket> m_acceptor;
    std::unique_ptr<TestStreamSocketDelegate> m_clientSocket;
    std::unique_ptr<TestStreamSocketDelegate> m_acceptedClientSocket;
    std::string m_lastError;
    nx::utils::promise<std::string> m_connectedPromise;
    nx::utils::future<std::string> m_connectedFuture;

    template<typename F>
    bool checkedCall(F f, const std::string& errorString);
    bool initAcceptor();
    bool initClientSocket();
    bool ok() const;
    void onAccept(
        SystemError::ErrorCode ecode,
        std::unique_ptr<AbstractStreamSocket> acceptedSocket);
    bool connect();
};

ConnectedSocketsSupplier::ConnectedSocketsSupplier():
    m_connectedFuture(m_connectedPromise.get_future())
{
    if(!initAcceptor())
        return;

    initClientSocket();
}

ConnectedSocketsSupplier::~ConnectedSocketsSupplier()
{
    if (m_acceptor)
        m_acceptor->pleaseStopSync();

    if (m_clientSocket)
        m_clientSocket->pleaseStopSync();

    if (m_acceptedClientSocket)
        m_acceptedClientSocket->pleaseStopSync();
}

bool ConnectedSocketsSupplier::initAcceptor()
{
    if (!checkedCall(
            [this]()
            {
                return (bool) (m_acceptor = std::unique_ptr<AbstractStreamServerSocket>(
                    SocketFactory::createStreamServerSocket()));
            },
            "SocketFactory::createStreamServerSocket"))
    {
        return false;
    }

    if (!checkedCall(
            [this]() { return m_acceptor->setNonBlockingMode(true); },
            "acceptor::setNonBlockingMode"))
    {
        return false;
    }

    if (!checkedCall(
            [this]() { return m_acceptor->bind(SocketAddress::anyPrivateAddress); },
            "acceptor::bind"))
    {
        return false;
    }

    return checkedCall(
            [this]() { return m_acceptor->listen(); },
            "acceptor::listen");
}

bool ConnectedSocketsSupplier::initClientSocket()
{
    if (!checkedCall(
            [this]()
            {
                return (bool) (m_clientSocket = std::make_unique<TestStreamSocketDelegate>(
                    SocketFactory::createStreamSocket(ssl::kAcceptAnyCertificate).release()));
            },
            "SocketFactory::createStreamSocket"))
    {
        return false;
    }

    return checkedCall(
            [this]() { return m_clientSocket->setNonBlockingMode(true); },
            "clientSocket::setNonBlockingMode");
}

template<typename F>
bool ConnectedSocketsSupplier::checkedCall(F f, const std::string& errorString)
{
    if (f())
        return true;

    m_lastError = errorString + " failed";
    return false;
}


bool ConnectedSocketsSupplier::connectSockets()
{
    if (!ok())
        return false;

    using namespace std::placeholders;
    m_acceptor->acceptAsync(std::bind(&ConnectedSocketsSupplier::onAccept, this, _1, _2));

    return connect();
}

bool ConnectedSocketsSupplier::connect()
{
    while (!m_clientSocket->connect(
                m_acceptor->getLocalAddress(),
                nx::network::deprecated::kDefaultConnectTimeout))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    m_lastError = m_connectedFuture.get();
    m_acceptor->pleaseStopSync();

    if (!ok())
        return false;

    return true;
}

void ConnectedSocketsSupplier::onAccept(
    SystemError::ErrorCode ecode,
    std::unique_ptr<AbstractStreamSocket> acceptedSocket)
{
    if (SystemError::noError != ecode)
    {
        m_connectedPromise.set_value("OnAccept error: " + std::to_string((int) ecode));
        return;
    }

    if (!acceptedSocket->setNonBlockingMode(true))
    {
        m_connectedPromise.set_value("OnAccept: acceptedSocket::setNonBlockingMode failed");
        return;
    }

    m_acceptedClientSocket = std::unique_ptr<TestStreamSocketDelegate>(
        new TestStreamSocketDelegate(acceptedSocket.release()));
    m_connectedPromise.set_value(std::string());
}

bool ConnectedSocketsSupplier::ok() const
{
    return m_lastError.empty();
}

std::unique_ptr<AbstractStreamSocket> ConnectedSocketsSupplier::clientSocket()
{
    return std::move(m_clientSocket);
}

std::unique_ptr<AbstractStreamSocket> ConnectedSocketsSupplier::serverSocket()
{
    return std::move(m_acceptedClientSocket);
}

std::string ConnectedSocketsSupplier::lastError()
{
    return m_lastError;
}
//-------------------------------------------------------------------------------------------------

class WebSocket:
    public ::testing::TestWithParam<CompressionType>
{
protected:
    WebSocket():
        clientSendBuf("hello"),
        readyFuture(readyPromise.get_future())
    {
    }

    ~WebSocket()
    {
        if (clientWebSocket)
            clientWebSocket->pleaseStopSync();
        if (serverWebSocket)
            serverWebSocket->pleaseStopSync();
    }

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_socketsSupplier.connectSockets()) << m_socketsSupplier.lastError();
    }

    void givenServerClientWebSockets(
        std::chrono::milliseconds clientTimeout, std::chrono::milliseconds serverTimeout,
        bool noPongs = false)
    {
        clientWebSocket.reset(new TestWebSocket(
            m_socketsSupplier.clientSocket(), clientSendMode, clientReceiveMode, Role::client,
            FrameType::binary, GetParam()));

        serverWebSocket.reset(new TestWebSocket(
            m_socketsSupplier.serverSocket(), serverSendMode, serverReceiveMode, serverRole,
            FrameType::binary, GetParam()));

        clientWebSocket->bindToAioThread(serverWebSocket->getAioThread());
        clientWebSocket->setAliveTimeout(clientTimeout);
        serverWebSocket->setAliveTimeout(serverTimeout);

        if (noPongs)
        {
            clientWebSocket->disablePingPong();
            serverWebSocket->disablePingPong();
        }

        clientWebSocket->start();
        serverWebSocket->start();
    }

    void givenServerClientWebSockets()
    {
        givenServerClientWebSockets(kAliveTimeout, kAliveTimeout);
    }

    void givenServerClientWebSocketsWithShortTimeout(bool noPongs)
    {
        givenClientModes(SendMode::singleMessage, ReceiveMode::message);
        givenServerModes(SendMode::singleMessage, ReceiveMode::message);
        givenServerClientWebSockets(kShortTimeout, kShortTimeout, noPongs);
    }

    void givenServerClientWebSocketsWithDifferentTimeouts()
    {
        givenClientModes(SendMode::singleMessage, ReceiveMode::message);
        givenServerModes(SendMode::singleMessage, ReceiveMode::message);
        givenServerClientWebSockets(kShortTimeout, kShortTimeout * 100);
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
            [this](SystemError::ErrorCode, size_t)
            {
                if (doneCount == kIterations)
                    return;
                clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);
            };

        clientReadCb =
            [this](SystemError::ErrorCode ecode, size_t)
            {
                ASSERT_EQ(ecode, SystemError::noError);
                if (++doneCount == kIterations)
                {
                    readyPromise.set_value();
                    return;
                }

                ASSERT_EQ(clientReadBuf, serverSendBuf);
                clientReadBuf.clear();
                clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
            };

        serverSendCb =
            [this](SystemError::ErrorCode ecode, size_t)
            {
                if (doneCount == kIterations)
                    return;

                ASSERT_EQ(ecode, SystemError::noError);
                serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
            };

        serverReadCb =
            [this](SystemError::ErrorCode error, size_t)
            {
                if (doneCount == kIterations)
                    return;

                ASSERT_EQ(error, SystemError::noError);
                ASSERT_EQ(serverReadBuf, clientSendBuf);
                serverReadBuf.clear();
                serverWebSocket->sendAsync(&serverSendBuf, serverSendCb);
            };
    }

    void whenClientSends(int messageCount)
    {
        auto readyPromise = std::make_shared<nx::utils::promise<void>>();
        auto readyFuture = readyPromise->get_future();
        int sentCount = 0;

        clientSendCb =
            [this, &sentCount, messageCount, readyPromise](
                SystemError::ErrorCode error, size_t) mutable
            {
                ASSERT_EQ(SystemError::noError, error);
                if (++sentCount == messageCount)
                {
                    readyPromise->set_value();
                    return;
                }
                clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
            };

        clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
        readyFuture.wait();
    }

    void whenServerStartsReading()
    {
        serverReadCb =
            [this](SystemError::ErrorCode error, size_t transferred)
            {
                ++doneCount;
                if (error != SystemError::noError || transferred == 0)
                    return;

                ASSERT_EQ(SystemError::noError, error);
                ASSERT_EQ(serverReadBuf, clientSendBuf);
                serverReadBuf.clear();
                serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
            };

        serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
    }

    void thenAllMessagesShouldBeReceived(int messageCount)
    {
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            nx::utils::promise<int> doneCountPromise;
            auto doneCountFuture = doneCountPromise.get_future();

            serverWebSocket->post(
                [this, doneCountPromise = std::move(doneCountPromise)]() mutable
                {
                    doneCountPromise.set_value(doneCount);
                });

            if (doneCountFuture.get() == messageCount)
                break;
        }
    }

    void whenServerDoesntAnswerPings() {}

    void whenReadWriteScheduled()
    {
        serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
    }

    template<typename SendAction>
    void assertCbErrorCode(SystemError::ErrorCode desiredEcode, SendAction sendAction)
    {
        auto p = std::make_shared<nx::utils::promise<void>>();
        auto f = p->get_future();
        SystemError::ErrorCode error = SystemError::noError;

        clientWebSocket->readSomeAsync(&clientReadBuf,
            [&error, p](SystemError::ErrorCode ecode, size_t) mutable
            {
                error = ecode;
                p->set_value();
            });

        sendAction();
        f.wait();
        ASSERT_EQ(desiredEcode, error);
    }

    void thenClientSocketShouldReceiveTimeoutErrorInReadHandler()
    {
        assertCbErrorCode(SystemError::timedOut, [](){});
    }

    void thenTransferFinishedSuccessfully()
    {
        readyFuture.wait();
    }

    void whenServerStartsAnsweringPings()
    {
        serverReadCb = [](SystemError::ErrorCode, size_t) {};
        serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
    }

    void thenNoNewErrorsShouldHappen()
    {
        assertCbErrorCode(SystemError::noError,
            [this]()
            {
                serverSendBuf = "hello";
                serverWebSocket->sendAsync(&serverSendBuf,
                    [](SystemError::ErrorCode ecode, size_t)
                    {
                        NX_ASSERT(ecode == SystemError::noError);
                    });
            });
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
                clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
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
                serverWebSocket->sendAsync(&serverSendBuf, serverSendCb);
            };
    }

    virtual void TearDown() override
    {
        stopSockets();
    }

    void stopSockets()
    {
        if (clientWebSocket)
            clientWebSocket->pleaseStopSync();
        if (serverWebSocket)
            serverWebSocket->pleaseStopSync();
    }

    void resetClientSocket()
    {
        clientWebSocket.reset();
        m_clientSocketDestroyed = true;
    }

    void resetServerSocket()
    {
        serverWebSocket.reset();
        m_serverSocketDestroyed = true;
    }

    void waitForClientSocketDestroyed()
    {
        while (!m_clientSocketDestroyed)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    void waitForServerSocketDestroyed()
    {
        while (!m_serverSocketDestroyed)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
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

    ConnectedSocketsSupplier m_socketsSupplier;

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

    std::atomic<bool> m_clientSocketDestroyed{false};
    std::atomic<bool> m_serverSocketDestroyed{false};

    std::promise<void> readyPromise;
    std::future<void> readyFuture;
    Role serverRole = Role::server;

    std::unique_ptr<TestWebSocket> clientWebSocket;
    std::unique_ptr<TestWebSocket> serverWebSocket;

    const std::chrono::milliseconds kAliveTimeout = std::chrono::milliseconds(100000);
    const std::chrono::milliseconds kShortTimeout = std::chrono::milliseconds(3000);
};

TEST_P(WebSocket, MultipleMessages_twoWay)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientServerExchangeMessagesCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerClientWebSockets();

    whenReadWriteScheduled();

    readyFuture.wait();
}

TEST_P(WebSocket, MultipleMessages_ReceiveModeFrame_twoWay)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientServerExchangeMessagesCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::frame);
    givenServerModes(SendMode::singleMessage, ReceiveMode::frame);
    givenServerClientWebSockets();

    whenReadWriteScheduled();

    thenTransferFinishedSuccessfully();
}

TEST_P(WebSocket, MultipleMessagesFromClient_ServerResponds)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenServerTestDataPrepared(2 * 1024 * 1024);
    givenClientManyMessagesWithServerRespondingCallbacks();
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerClientWebSockets();

    whenReadWriteScheduled();
    clientWebSocket->readSomeAsync(&clientReadBuf, clientReadCb);

    readyFuture.wait();
}

INSTANTIATE_TEST_SUITE_P(
    Websockets_differentCompressionModes,
    WebSocket,
    ::testing::Values(CompressionType::none, CompressionType::perMessageDeflate));

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
                    return cf::sendAsync(state->webSocket.get(), &state->buffer).then(
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
            return cf::sendAsync(state->webSocket.get(), &state->buffer).then(
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

TEST_P(WebSocket, Wrappers)
{
    givenClientTestDataPrepared(1 * 1024 * 1024);
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerClientWebSockets();

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
        givenServerTestDataPrepared(1024 * 1024 * 3);

        clientSendCb =
            [&](SystemError::ErrorCode ecode, size_t)
            {
                if (ecode != SystemError::noError)
                {
                    processError(ecode);
                    resetClientSocket();
                    return;
                }

                ++sentMessageCount;

                if (sentMessageCount >= kTotalMessageCount)
                {
                    try { readyPromise.set_value(); } catch (...) {}
                    resetClientSocket();
                    return;
                }

                clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
            };

        clientReadCb =
            [&](SystemError::ErrorCode ecode, size_t transferred)
            {
                if (ecode != SystemError::noError || transferred == 0)
                {
                    processError(ecode);
                    resetClientSocket();
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
                    processError(ecode);
                    resetServerSocket();
                    return;
                }

                if (!sendQueue.empty())
                {
                    auto bufToSend = sendQueue.front();
                    sendQueue.pop();
                    serverWebSocket->sendAsync(&bufToSend, serverSendCb);
                }
            };

        serverReadCb =
            [&](SystemError::ErrorCode ecode, size_t transferred)
            {

                if (ecode != SystemError::noError || transferred == 0)
                {
                    processError(ecode);
                    resetServerSocket();
                    return;
                }

                serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
                if (isServerResponding)
                {
                    if (sendQueue.empty())
                        serverWebSocket->sendAsync(&serverReadBuf, serverSendCb);
                    else
                        sendQueue.push(serverReadBuf);
                }
                serverReadBuf.clear();
            };
    }

    virtual void TearDown() override
    {
        if (m_idleThread.joinable())
            m_idleThread.join();

        WebSocket::TearDown();
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
        if (isClientSending)
            clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
        serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

        if (beforeWaitAction)
            beforeWaitAction();

        readyFuture.wait();
    }

    void startAndWaitForDestroyed()
    {
        start();
        waitForClientSocketDestroyed();
        waitForServerSocketDestroyed();
    }

    void whenConnectionIsIdleForSomeTime()
    {
        m_idleThread = std::thread(
            [this]()
            {
                std::this_thread::sleep_for(kShortTimeout * 4);
                try { readyPromise.set_value(); } catch (...) {}
            });

        start();
        readyFuture.wait();
        if (clientWebSocket)
            clientWebSocket->pleaseStopSync();
        waitForServerSocketDestroyed();
    }

    void thenItsBeenKeptAliveByThePings()
    {
        ASSERT_FALSE(isTimeoutError);
    }

    void thenConnectionShouldBeAbortedWithTimeout()
    {
        ASSERT_TRUE(isTimeoutError);
    }

    std::function<void()> beforeWaitAction;

    bool isServerResponding = true;
    bool isClientSending = true;
    bool isTimeoutError = false;
    int sentMessageCount = 0;
    const int kTotalMessageCount = 30;
    std::queue<nx::Buffer> sendQueue;
    std::thread m_idleThread;
};

TEST_P(WebSocket_PingPong, PingPong_noPingsBecauseOfData)
{
    givenServerClientWebSockets();
    startAndWaitForDestroyed();
    ASSERT_FALSE(isTimeoutError);
}

TEST_P(WebSocket_PingPong, PingPong_pingsBecauseOfNoData)
{
    isServerResponding = false;
    isClientSending = false;

    givenServerClientWebSocketsWithShortTimeout(/*noPongs*/ false);
    whenConnectionIsIdleForSomeTime();
    thenItsBeenKeptAliveByThePings();
}

TEST_P(WebSocket_PingPong, PingPong_abortIfNoPongs)
{
    isServerResponding = false;
    isClientSending = false;

    givenServerClientWebSocketsWithShortTimeout(/*noPongs*/ true);
    whenConnectionIsIdleForSomeTime();
    thenConnectionShouldBeAbortedWithTimeout();
}

TEST_P(WebSocket_PingPong, Close)
{
    givenServerClientWebSockets();
    isClientSending = false;
    beforeWaitAction =
        [this]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            clientWebSocket->sendCloseAsync();
        };

    startAndWaitForDestroyed();
    ASSERT_FALSE(clientWebSocket);
    ASSERT_FALSE(serverWebSocket);
}

TEST_P(WebSocket, SendMultiFrame_ReceiveSingleMessage)
{
    givenClientModes(SendMode::multiFrameMessage, ReceiveMode::frame);
    givenServerModes(SendMode::multiFrameMessage, ReceiveMode::message);
    givenServerClientWebSockets();

    int frameCount = 1;
    int sentMessageCount = 0;
    int receivedMessageCount = 0;
    const int kMessageFrameCount = 100;
    const int kTotalMessageCount = 20;

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t)
        {
            ASSERT_EQ(ecode, SystemError::noError);
            frameCount++;
            if (frameCount == kMessageFrameCount)
            {
                clientWebSocket->setIsLastFrame();
                frameCount = 0;
                sentMessageCount++;
            }
            clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
        };

    serverReadCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (ecode != SystemError::noError || transferred == 0)
                return;

            ASSERT_EQ(clientSendBuf.size() * kMessageFrameCount, serverReadBuf.size());

            receivedMessageCount++;
            if (receivedMessageCount >= kTotalMessageCount)
            {
                readyPromise.set_value();
                return;
            }

            serverReadBuf.clear();
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };

    whenReadWriteScheduled();

    readyFuture.wait();
    stopSockets();

    ASSERT_EQ(kTotalMessageCount, receivedMessageCount);
    ASSERT_GE(sentMessageCount, receivedMessageCount);
}

TEST_P(WebSocket, SendMultiFrame_ReceiveFrame)
{
    givenClientModes(SendMode::multiFrameMessage, ReceiveMode::frame);
    givenServerModes(SendMode::multiFrameMessage, ReceiveMode::frame);
    givenServerClientWebSockets();

    int frameCount = 0;
    int sentMessageCount = 0;
    int receivedFrameCount = 0;
    const int kMessageFrameCount = 100;
    const int kTotalMessageCount = 20;

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

            clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
        };

    serverReadCb =
        [&](SystemError::ErrorCode ecode, size_t transferred)
        {
            if (ecode != SystemError::noError || transferred == 0)
                return;
            ASSERT_EQ(serverReadBuf.size(), clientSendBuf.size());
            receivedFrameCount++;

            if (receivedFrameCount >= kTotalMessageCount*kMessageFrameCount)
            {
                readyPromise.set_value();
                return;
            }

            serverReadBuf.clear();
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };

    clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
    serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);

    readyFuture.wait();
    stopSockets();

    ASSERT_EQ(receivedFrameCount, kTotalMessageCount*kMessageFrameCount);
}

TEST_P(WebSocket_PingPong, UnexpectedClose_deleteFromCb_send)
{
    givenServerClientWebSockets();
    beforeWaitAction =
        [this]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            clientWebSocket->socket()->terminate();
        };

    startAndWaitForDestroyed();

    ASSERT_FALSE(clientWebSocket);
    ASSERT_FALSE(serverWebSocket);
}

TEST_P(WebSocket_PingPong, UnexpectedClose_deleteFromCb_receive)
{
    givenServerClientWebSockets();
    beforeWaitAction =
        [this]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            serverWebSocket->socket()->terminate();
        };

    startAndWaitForDestroyed();

    ASSERT_FALSE(clientWebSocket);
    ASSERT_FALSE(serverWebSocket);
}

TEST_P(WebSocket, UnexpectedClose_ReadReturnedZero)
{
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenClientTestDataPrepared(1684 * 1024 + 17);
    givenServerClientWebSockets();

    int sentMessageCount = 0;
    const int kTotalMessageCount = 100;

    clientSendCb =
        [&](SystemError::ErrorCode ecode, size_t)
        {
            if (ecode != SystemError::noError)
            {
                try { readyPromise.set_value(); } catch (...) {}
                resetClientSocket();
                return;
            }
            sentMessageCount++;
            if (sentMessageCount >= kTotalMessageCount)
            {
                readyPromise.set_value();
                return;
            }
            clientWebSocket->sendAsync(&clientSendBuf, clientSendCb);
        };

    serverReadCb =
        [this](SystemError::ErrorCode, size_t bytesRead)
        {
            if (bytesRead == 0)
            {
                try { readyPromise.set_value(); } catch (...) {}
                resetServerSocket();
                return;
            }
            serverReadBuf.clear();
            serverWebSocket->readSomeAsync(&serverReadBuf, serverReadCb);
        };

    whenReadWriteScheduled();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    serverWebSocket->socket()->setZeroRead();

    readyFuture.wait();
    waitForClientSocketDestroyed();
    waitForServerSocketDestroyed();

    ASSERT_TRUE(!serverWebSocket);
}

TEST_P(WebSocket, SendAfterConnectionHasFailed)
{
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenClientTestDataPrepared(1684 * 1024 + 17);
    givenServerClientWebSockets();

    serverWebSocket->post([this]() { resetServerSocket(); });
    utils::promise<SystemError::ErrorCode> failHandlerPromise;
    auto failHandlerFuture = failHandlerPromise.get_future();

    clientWebSocket->sendAsync(
        &clientSendBuf,
        [p = std::move(failHandlerPromise)](SystemError::ErrorCode error, size_t) mutable
        {
            p.set_value(error);
        });

    ASSERT_EQ(SystemError::connectionAbort, failHandlerFuture.get());
    clientWebSocket->post([this]() { resetClientSocket(); });

    waitForClientSocketDestroyed();
    waitForServerSocketDestroyed();
}

TEST_P(WebSocket, imcomingMessagesQueueOverflow)
{
    givenClientModes(SendMode::singleMessage, ReceiveMode::message);
    givenServerModes(SendMode::singleMessage, ReceiveMode::message);
    givenClientTestDataPrepared(100);
    givenServerClientWebSockets();

    whenClientSends(/*messageCount*/1100);
    whenServerStartsReading();
    thenAllMessagesShouldBeReceived(/*messageCount*/1100);
}

INSTANTIATE_TEST_SUITE_P(Websockets_PingPong_differentCompressionModes,
    WebSocket_PingPong,
    ::testing::Values(CompressionType::none, CompressionType::perMessageDeflate));

} // namespace nx::network::websocket::test
