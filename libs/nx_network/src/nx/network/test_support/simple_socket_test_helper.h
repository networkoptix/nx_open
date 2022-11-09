// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>
#include <optional>

#include <nx/network/abstract_socket.h>
#include <nx/network/async_stoppable.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace network {
namespace test {

namespace {

const nx::Buffer kTestMessage("Ping");
const std::chrono::milliseconds kTestTimeout(5000);

struct FunctionsTag{};
const utils::log::Tag kLogTag(typeid(FunctionsTag));

inline size_t testClientCount()
{
     return nx::utils::TestOptions::applyLoadMode<size_t>(10);
}

static const bool kEnableTestDebugOutput = false;
template<typename Message>
static inline void testDebugOutput(const Message& message)
{
    if (kEnableTestDebugOutput)
        NX_DEBUG(kLogTag, nx::format("%1"), message);
}

} // namespace

template<typename SocketType>
nx::Buffer readNBytes(SocketType* clientSocket, std::size_t count)
{
    const auto BUF_SIZE = std::max<std::size_t>(count, 128);

    nx::Buffer buffer(BUF_SIZE, char(0));
    std::size_t bufDataSize = 0;
    for (;;)
    {
        const auto bytesRead = clientSocket->recv(
            buffer.data() + bufDataSize,
            buffer.size() - bufDataSize,
            0);
        if (bytesRead <= 0)
            return buffer.substr(0, bufDataSize);

        bufDataSize += bytesRead;
        if (bufDataSize >= count)
            return buffer.substr(0, bufDataSize);
    }
}


enum class ErrorHandling
{
    ignore,
    triggerAssert
};

template<typename ServerSocketType>
class SyncSocketServer
{
public:
    struct ServerStartResult
    {
        bool hasSucceeded;
        SocketAddress address;

        ServerStartResult(
            bool hasSucceeded,
            SocketAddress address)
            :
            hasSucceeded(hasSucceeded),
            address(std::move(address))
        {
        }
    };

    SyncSocketServer(ServerSocketType server):
        m_server(std::move(server))
    {
    }

    void setEndpointToBindTo(SocketAddress endpoint)
    {
        m_endpointToBindTo = std::move(endpoint);
    }

    void setTestMessage(Buffer value)
    {
        m_testMessage = std::move(value);
    }

    void setErrorHandling(ErrorHandling value)
    {
        m_errorHandling = value;
    }

    SocketAddress start()
    {
        m_thread = std::make_unique<nx::utils::thread>([this](){ run(); });
        const auto result = m_startedPromise.get_future().get();
        NX_GTEST_ASSERT_TRUE(result.hasSucceeded);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return result.address;
    }

    ~SyncSocketServer()
    {
        m_needToStop = true;
        if (m_thread)
            m_thread->join();
    }

private:
    void run()
    {
        auto startedPromiseGuard = nx::utils::makeScopeGuard(
            [this]() { m_startedPromise.set_value(ServerStartResult(false, SocketAddress())); });

        ASSERT_TRUE(m_server->setReuseAddrFlag(true)) << SystemError::getLastOSErrorText();
        ASSERT_TRUE(m_server->bind(m_endpointToBindTo)) << SystemError::getLastOSErrorText();
        ASSERT_TRUE(m_server->listen((int)testClientCount())) << SystemError::getLastOSErrorText();

        ASSERT_TRUE(m_server->setRecvTimeout(100)) << SystemError::getLastOSErrorText();
        auto client = m_server->accept();
        ASSERT_FALSE(client);
        ASSERT_EQ(SystemError::timedOut, SystemError::getLastOSErrorCode());

        auto serverAddress = m_server->getLocalAddress();

        NX_INFO(this, nx::format("Started on %1").arg(serverAddress.toString()));
        startedPromiseGuard.disarm();
        m_startedPromise.set_value(ServerStartResult(true, std::move(serverAddress)));

        const auto startTime = std::chrono::steady_clock::now();
        const auto maxTimeout = kTestTimeout * testClientCount();
        while (!m_needToStop)
        {
            std::unique_ptr<AbstractStreamSocket> client(m_server->accept());
            if (!client)
            {
                if (SystemError::getLastOSErrorCode() == SystemError::timedOut ||
                    m_errorHandling == ErrorHandling::ignore)
                {
                    const auto runTime = std::chrono::steady_clock::now() - startTime;
                    NX_ASSERT( runTime < maxTimeout, "Are we frozen?");
                    continue;
                }
            }

            ASSERT_TRUE(client.get()) << SystemError::getLastOSErrorText();
            ASSERT_TRUE(client->setRecvTimeout(kTestTimeout.count()));
            ASSERT_TRUE(client->setSendTimeout(kTestTimeout.count()));

            if (m_testMessage.empty())
                continue;

            const auto incomingMessage = readNBytes(client.get(), m_testMessage.size());
            if (m_errorHandling == ErrorHandling::triggerAssert)
            {
                ASSERT_TRUE(!incomingMessage.empty()) << SystemError::getLastOSErrorText();
                ASSERT_EQ(m_testMessage, incomingMessage);
            }

            const int bytesSent = client->send(m_testMessage);
            if (m_errorHandling == ErrorHandling::triggerAssert)
            {
                ASSERT_NE(-1, bytesSent) << SystemError::getLastOSErrorText();
            }

            //waiting for connection to be closed by client
            nx::Buffer buf(64, 0);
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

    ServerSocketType m_server;
    SocketAddress m_endpointToBindTo = SocketAddress::anyPrivateAddress;
    Buffer m_testMessage = kTestMessage;
    nx::utils::promise<ServerStartResult> m_startedPromise;
    ErrorHandling m_errorHandling{ErrorHandling::triggerAssert};
    std::atomic<bool> m_needToStop{false};
    std::unique_ptr<nx::utils::thread> m_thread;
};

template<typename Socket, typename ... Args>
std::unique_ptr<SyncSocketServer<Socket>> syncSocketServer(Socket socket, Args ... args)
{
    return std::make_unique<SyncSocketServer<Socket>>(
        std::move(socket), std::forward<Args>(args) ...);
}

template<typename Sender, typename Receiver>
inline void transferSyncAsync(Sender* sender, Receiver* receiver)
{
    ASSERT_EQ(kTestMessage.size(), sender->AbstractCommunicatingSocket::send(kTestMessage))
        << SystemError::getLastOSErrorText();

    Buffer buffer;
    buffer.reserve(kTestMessage.size());
    nx::utils::promise<std::tuple<SystemError::ErrorCode, size_t>> promise;
    receiver->readAsyncAtLeast(
        &buffer, kTestMessage.size(),
        [&](SystemError::ErrorCode code, size_t bytesRead)
        {
            promise.set_value(std::make_tuple(code, bytesRead));
        });
    const auto result = promise.get_future().get();

    ASSERT_EQ(SystemError::noError, std::get<0>(result))
        << SystemError::toString(std::get<0>(result));
    ASSERT_EQ((size_t)kTestMessage.size(), std::get<1>(result));
    ASSERT_EQ(buffer, kTestMessage);
}

template<typename Sender, typename Receiver>
static inline void transferAsyncSync(Sender* sender, Receiver* receiver)
{
    nx::utils::promise<std::tuple<SystemError::ErrorCode, size_t>> promise;
    sender->sendAsync(
        &kTestMessage,
        [&](SystemError::ErrorCode code, size_t size)
        {
            promise.set_value(std::make_tuple(code, size));
        });

    Buffer buffer(kTestMessage.size(), 0);
    ASSERT_EQ(buffer.size(), receiver->recv(buffer.data(), buffer.size(), MSG_WAITALL))
        << SystemError::getLastOSErrorText();
    ASSERT_EQ(buffer, kTestMessage);

    const auto sendResult = promise.get_future().get();
    ASSERT_EQ(SystemError::noError, std::get<0>(sendResult))
        << SystemError::toString(std::get<0>(sendResult));
    ASSERT_EQ((size_t)kTestMessage.size(), std::get<1>(sendResult));
}

template<typename Sender, typename Receiver>
static inline void transferSync(Sender* sender, Receiver* receiver)
{
    ASSERT_EQ(sender->AbstractCommunicatingSocket::send(kTestMessage), kTestMessage.size())
        << SystemError::getLastOSErrorText();

    Buffer buffer(kTestMessage.size(), 0);
    EXPECT_EQ(buffer.size(), receiver->recv(buffer.data(), buffer.size(), MSG_WAITALL))
        << SystemError::getLastOSErrorText();
}

template<typename Sender, typename Receiver>
static inline void transferAsync(Sender* sender, Receiver* receiver)
{
    nx::utils::promise<std::tuple<SystemError::ErrorCode, size_t>> sendPromise;
    sender->sendAsync(
        &kTestMessage,
        [&](SystemError::ErrorCode code, size_t size)
        {
            sendPromise.set_value(std::make_tuple(code, size));
        });

    Buffer buffer;
    buffer.reserve(kTestMessage.size());
    nx::utils::promise<std::tuple<SystemError::ErrorCode, size_t>> readPromise;
    receiver->readAsyncAtLeast(
        &buffer, kTestMessage.size(),
        [&](SystemError::ErrorCode code, size_t size)
        {
            readPromise.set_value(std::make_tuple(code, size));
        });

    const auto sendResult = sendPromise.get_future().get();
    ASSERT_EQ(SystemError::noError, std::get<0>(sendResult))
        << SystemError::toString(std::get<0>(sendResult));
    ASSERT_EQ((size_t)kTestMessage.size(), std::get<1>(sendResult));

    const auto readResult = readPromise.get_future().get();
    ASSERT_EQ(SystemError::noError, std::get<0>(readResult))
        << SystemError::toString(std::get<0>(readResult));
    ASSERT_EQ((size_t)kTestMessage.size(), std::get<1>(readResult));
    ASSERT_EQ(kTestMessage, buffer);
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketSyncAsyncSwitch(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    bool bothNonBlocking,
    std::optional<SocketAddress> endpointToConnectTo = std::nullopt)
{
    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(100));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress))
        << SystemError::getLastOSErrorText();
    ASSERT_TRUE(server->listen((int)testClientCount()))
        << SystemError::getLastOSErrorText();

    auto serverAddress = server->getLocalAddress();
    NX_DEBUG(kLogTag, nx::format("Server address: %1").arg(serverAddress.toString()));
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    ASSERT_FALSE((bool) server->accept()) << SystemError::getLastOSErrorText();

    auto client = clientMaker();
    const auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });
    ASSERT_TRUE(client->setNonBlockingMode(true));

    nx::utils::promise<SystemError::ErrorCode> connectPromise;
    client->connectAsync(
        *endpointToConnectTo,
        [&](SystemError::ErrorCode code)
        {
            connectPromise.set_value(code);
        });
    ASSERT_EQ(SystemError::noError, connectPromise.get_future().get());

    // A finite timeout does not work here because client socket reports connected
    // just after sending ACK to the server.
    // But server reports accepted socket only after receiving that ACK.
    ASSERT_TRUE(server->setRecvTimeout(kNoTimeout.count()));

    auto accepted = server->accept();
    ASSERT_TRUE((bool) accepted) << SystemError::getLastOSErrorText();
    const auto acceptedGuard = nx::utils::makeScopeGuard(
        [&accepted]() { accepted->pleaseStopSync(); });

    // Overwriting inherited socket timeout.
    ASSERT_TRUE(accepted->setRecvTimeout(nx::network::kNoTimeout));
    transferAsyncSync(client.get(), accepted.get());
    transferSyncAsync(accepted.get(), client.get());

    if (bothNonBlocking)
    {
        ASSERT_TRUE(accepted->setNonBlockingMode(true));
        transferAsync(client.get(), accepted.get());
        transferAsync(accepted.get(), client.get());
        ASSERT_TRUE(client->setNonBlockingMode(false));
    }
    else
    {
        ASSERT_TRUE(client->setNonBlockingMode(false));
        transferSync(client.get(), accepted.get());
        transferSync(accepted.get(), client.get());
        ASSERT_TRUE(accepted->setNonBlockingMode(true));
    }

    transferSyncAsync(client.get(), accepted.get());
    transferAsyncSync(accepted.get(), client.get());
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketTransferFragmentation(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    std::optional<SocketAddress> endpointToConnectTo = std::nullopt)
{
    // On localhost TCP connection small packets usually transferred entirely,
    // so that we expect the same behavior for all our stream sockets.
    static const Buffer kMessage(utils::random::generate(100));
    static const size_t kTestRuns = utils::TestOptions::applyLoadMode<size_t>(5);

    auto server = serverMaker();
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress))
        << SystemError::getLastOSErrorText();
    ASSERT_TRUE(server->listen((int)testClientCount()))
        << SystemError::getLastOSErrorText();

    auto serverAddress = server->getLocalAddress();
    NX_DEBUG(kLogTag, nx::format("Server address: %1").arg(serverAddress.toString()));
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    auto client = clientMaker();
    ASSERT_TRUE(client->connect(*endpointToConnectTo, nx::network::kNoTimeout))
        << SystemError::getLastOSErrorText();
    ASSERT_TRUE(client->setNonBlockingMode(true));
    const auto clientGuard = nx::utils::makeScopeGuard([&](){ client->pleaseStopSync(); });

    auto accepted = server->accept();
    ASSERT_NE(nullptr, accepted)
        << SystemError::getLastOSErrorText();

    for (size_t runNumber = 0; runNumber <= kTestRuns; ++runNumber)
    {
        NX_DEBUG(kLogTag, nx::format("Start transfer %1").arg(runNumber));
        nx::utils::promise<std::tuple<SystemError::ErrorCode, size_t>> sendPromise;
        client->sendAsync(
            &kMessage,
            [&sendPromise](SystemError::ErrorCode code, size_t size)
            {
                sendPromise.set_value(std::make_tuple(code, size));
            });

        Buffer buffer(kMessage.size(), 0);
        ASSERT_EQ(buffer.size(), accepted->recv(buffer.data(), buffer.size()))
            << SystemError::getLastOSErrorText();
        ASSERT_EQ(buffer, kMessage);

        const auto sendResult = sendPromise.get_future().get();
        ASSERT_EQ(SystemError::noError, std::get<0>(sendResult))
            << SystemError::toString(std::get<0>(sendResult));
        ASSERT_EQ((size_t)kMessage.size(), std::get<1>(sendResult));
    }
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void socketMultiConnect(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker,
    std::optional<SocketAddress> endpointToConnectTo = std::nullopt)
{
    nx::Mutex connectedSocketsMutex;
    bool terminated = false;
    nx::utils::TestSyncQueue< SystemError::ErrorCode > acceptResults;
    nx::utils::TestSyncQueue< SystemError::ErrorCode > connectResults;
    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedSockets;
    std::vector<std::unique_ptr<AbstractStreamSocket>> connectedSockets;
    decltype(serverMaker()) server;

    std::function<void(SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>)> acceptor =
        [&](SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
        {
            acceptResults.push(code);
            if (code != SystemError::noError)
                return;

            acceptedSockets.emplace_back(std::move(socket));
            server->acceptAsync(acceptor);
        };

    std::function<void(int)> connectNewClients =
        [&](int clientsToConnect)
        {
            NX_MUTEX_LOCKER lock(&connectedSocketsMutex);

            if (clientsToConnect == 0 || terminated)
                return;

            auto testClient = clientMaker();
            ASSERT_TRUE(testClient->setNonBlockingMode(true));
            connectedSockets.push_back(std::move(testClient));

            connectedSockets.back()->connectAsync(
                *endpointToConnectTo,
                [clientsToConnect, connectNewClients, &connectResults]
                    (SystemError::ErrorCode code)
                {
                    connectResults.push(code);
                    if (code == SystemError::noError)
                        connectNewClients(clientsToConnect - 1);
                });
        };

    server = serverMaker();
    auto serverGuard = nx::utils::makeScopeGuard([&server]() { server->pleaseStopSync(); });
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress)) << SystemError::getLastOSErrorText();
    ASSERT_TRUE(server->listen((int)testClientCount())) << SystemError::getLastOSErrorText();

    auto connectedSocketsGuard = nx::utils::makeScopeGuard(
        [&connectedSockets, &connectedSocketsMutex, &terminated]()
        {
            {
                NX_MUTEX_LOCKER lock(&connectedSocketsMutex);
                terminated = true;
            }

            for (auto& socket: connectedSockets)
                socket->pleaseStopSync();
        });

    auto serverAddress = server->getLocalAddress();
    NX_DEBUG(kLogTag, nx::format("Server address: %1").arg(serverAddress.toString()));
    if (!endpointToConnectTo)
        endpointToConnectTo = std::move(serverAddress);

    server->acceptAsync(acceptor);
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // for UDT only

    const auto kClientCount = testClientCount();
    connectNewClients((int)kClientCount);
    for (size_t i = 0; i < kClientCount; ++i)
    {
        ASSERT_EQ(acceptResults.pop(), SystemError::noError);
        ASSERT_EQ(connectResults.pop(), SystemError::noError);
    }
}

template<typename ServerSocketMaker, typename ClientSocketMaker>
void acceptedSocketOptionsInheritance(
    const ServerSocketMaker& serverMaker,
    const ClientSocketMaker& clientMaker)
{
    auto server = serverMaker();
    auto serverGuard = nx::utils::makeScopeGuard([&](){ server->pleaseStopSync(); });
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->setRecvTimeout(10 * 1000));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen((int)testClientCount()));
    auto serverAddress = server->getLocalAddress();

    auto client = clientMaker();
    ASSERT_TRUE(client->connect(serverAddress, nx::network::kNoTimeout));

    auto accepted = server->accept();
    ASSERT_TRUE((bool)accepted);

    unsigned int acceptedSocketRecvTimeout = 0;
    ASSERT_TRUE(accepted->getRecvTimeout(&acceptedSocketRecvTimeout));
    unsigned int acceptedSocketSendTimeout = 0;
    ASSERT_TRUE(accepted->getSendTimeout(&acceptedSocketSendTimeout));

    // Timeouts are not inherited.
    ASSERT_EQ(0U, acceptedSocketRecvTimeout);
    ASSERT_EQ(0U, acceptedSocketSendTimeout);
}

template<typename ClientSocketMaker>
void multipleSocketsBoundToTheSameThreadReportCompletionWithinSameThread(const ClientSocketMaker& clientMaker)
{
    aio::AbstractAioThread* aioThread(nullptr);
    nx::utils::TestSyncQueue<nx::utils::thread::id> threadIdQueue;
    std::vector<decltype(clientMaker())> sockets;
    auto socketsGuard = nx::utils::makeScopeGuard(
        [&sockets]()
        {
            for (auto& each: sockets)
                each->pleaseStopSync();
        });

    const auto clientCount = testClientCount();
    for (size_t i = 0; i < clientCount; ++i)
    {
        auto client = clientMaker();
        ASSERT_TRUE(client->setNonBlockingMode(true));
        ASSERT_TRUE(client->setSendTimeout(100));
        ASSERT_TRUE(client->setRecvTimeout(100));

        if (aioThread)
            client->bindToAioThread(aioThread);
        else
            aioThread = client->getAioThread();

        client->connectAsync(
            "12.34.56.78:9999",
            [&](SystemError::ErrorCode /*code*/)
            {
                threadIdQueue.push(std::this_thread::get_id());
            });

        sockets.push_back(std::move(client));
    }

    std::optional<nx::utils::thread::id> aioThreadId;
    for (size_t i = 0; i < clientCount; ++i)
    {
        const auto threadId = threadIdQueue.pop();
        if (aioThreadId)
            ASSERT_EQ(*aioThreadId, threadId);
        else
            aioThreadId = threadId;
    }
}

template<typename ClientSocketMaker>
void socketConnectToBadAddress(
    const ClientSocketMaker& clientMaker,
    bool deleteInIoThread)
{
    const std::chrono::seconds sendTimeout(1);

    auto client = clientMaker();
    auto clientGuard = nx::utils::makeScopeGuard(
        [&client]() { if (client) client->pleaseStopSync(); });
    ASSERT_TRUE(client->setNonBlockingMode(true));
    ASSERT_TRUE(client->setSendTimeout(
        std::chrono::duration_cast<std::chrono::milliseconds>(sendTimeout).count()));

    nx::utils::promise<SystemError::ErrorCode> connectCompletedPromise;
    client->connectAsync(
        SocketAddress("f9094d31-ae48-4e69-9ad4-3aa91488df3.com:123"),
        [&](SystemError::ErrorCode errorCode)
        {
            if (deleteInIoThread)
                client.reset();

            connectCompletedPromise.set_value(errorCode);
        });

    const auto errorCode = connectCompletedPromise.get_future().get();
    ASSERT_NE(SystemError::noError, errorCode);
}

enum class StopType { cancelIo, pleaseStop };

template<typename ClientSocketMaker>
void socketIsValidAfterPleaseStop(const ClientSocketMaker& clientMaker)
{
    auto socket = clientMaker();
    ASSERT_TRUE(socket->setNonBlockingMode(true));
    socket->connectAsync(
        SocketAddress(HostAddress::localhost, 12345),
        [](SystemError::ErrorCode /*sysErrorCode*/) {});

    socket->pleaseStopSync();
    socket->setRecvBufferSize(128 * 1024);
}

template<typename ServerSocketMaker>
void socketAcceptCancelSync(
    const ServerSocketMaker& serverMaker, StopType stopType)
{
    auto server = serverMaker();
    auto serverGuard = nx::utils::makeScopeGuard([&server]() { server->pleaseStopSync(); });
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->setRecvTimeout(kTestTimeout.count()));
    ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(server->listen(5));

    const auto serverCount = testClientCount();
    for (size_t i = 0; i < serverCount; ++i)
    {
        server->acceptAsync(
            [&](SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>) { /*pass*/ });
        if (i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));

        switch (stopType)
        {
            case StopType::cancelIo:
                server->cancelIOSync();
                break;
            case StopType::pleaseStop:
                server->pleaseStopSync();
                return; // is not recoverable
            default:
                NX_CRITICAL(false);
        };
    }
}

template<typename ServerSocketMaker>
void serverSocketPleaseStopCancelsPostedCall(
    const ServerSocketMaker& serverMaker)
{
    auto serverSocket = serverMaker();
    nx::utils::promise<void> socketStopped;
    serverSocket->post(
        [&serverSocket, &socketStopped]()
        {
            serverSocket->post([]() { ASSERT_TRUE(false); });
            serverSocket->pleaseStopSync();
            socketStopped.set_value();
        });
    socketStopped.get_future().wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

//-------------------------------------------------------------------------------------------------

template<typename CreateSocketFunc>
class SocketOptions:
    public ::testing::Test
{
public:
    using SocketPointer = typename std::invoke_result_t<CreateSocketFunc>;

protected:
    std::vector<SocketPointer> m_sockets;

    void givenSocketBoundToLocalAddress()
    {
        addSocket();
        ASSERT_TRUE(m_sockets.back()->bind(SocketAddress::anyPrivateAddress));
    }

    void assertAnotherSocketCanBeBoundToTheSameAddress()
    {
        addSocket();
        ASSERT_TRUE(m_sockets.back()->bind(m_sockets.front()->getLocalAddress()));
    }

    void assertAnotherSocketCannotBeBoundToTheSameAddress()
    {
        addSocket();
        ASSERT_FALSE(m_sockets.back()->bind(m_sockets.front()->getLocalAddress()));
    }

    void addSocket()
    {
        m_sockets.push_back(CreateSocketFunc()());
        if (m_reuseAddr)
        {
            ASSERT_TRUE(m_sockets.back()->setReuseAddrFlag(true));
            ASSERT_TRUE(m_sockets.back()->setReusePortFlag(true));
        }
    }

    void enableReuseAddr()
    {
        m_reuseAddr = true;
    }

private:
    bool m_reuseAddr = false;
};

TYPED_TEST_SUITE_P(SocketOptions);

TYPED_TEST_P(SocketOptions, same_address_can_be_used_if_reuse_addr_enabled)
{
    this->enableReuseAddr();

    this->givenSocketBoundToLocalAddress();
    this->assertAnotherSocketCanBeBoundToTheSameAddress();
}

TYPED_TEST_P(SocketOptions, same_address_cannot_be_used_by_default)
{
    this->givenSocketBoundToLocalAddress();
    this->assertAnotherSocketCannotBeBoundToTheSameAddress();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SocketOptions);
REGISTER_TYPED_TEST_SUITE_P(SocketOptions,
    same_address_can_be_used_if_reuse_addr_enabled,
    same_address_cannot_be_used_by_default);

//-------------------------------------------------------------------------------------------------

template<typename CreateSocketFunc>
class SocketOptionsDefaultValue:
    public SocketOptions<CreateSocketFunc>
{
public:
    SocketOptionsDefaultValue()
    {
        this->addSocket();
    }

protected:
    template<typename SocketInterface, typename SocketOption>
    SocketOption getOptionValue(
        bool (SocketInterface::*getSocketOption)(SocketOption*) const)
    {
        auto value = SocketOption();
        NX_GTEST_ASSERT_TRUE((this->m_sockets.front().get()->*getSocketOption)(&value));
        return value;
    }
};

TYPED_TEST_SUITE_P(SocketOptionsDefaultValue);

TYPED_TEST_P(SocketOptionsDefaultValue, reuse_addr)
{
    ASSERT_EQ(false, this->getOptionValue(&AbstractSocket::getReuseAddrFlag));
}

TYPED_TEST_P(SocketOptionsDefaultValue, non_blocking_mode)
{
    ASSERT_EQ(false, this->getOptionValue(&AbstractSocket::getNonBlockingMode));
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SocketOptionsDefaultValue);
REGISTER_TYPED_TEST_SUITE_P(SocketOptionsDefaultValue,
    reuse_addr,
    non_blocking_mode);

} // namespace test
} // namespace network
} // namespace nx

typedef nx::network::test::StopType StopType;

#define NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, SingleAioThread) \
        { nx::network::test::multipleSocketsBoundToTheSameThreadReportCompletionWithinSameThread(mkClient); } \
    Type(Name, ConnectToBadAddress) \
        { nx::network::test::socketConnectToBadAddress(mkClient, false); } \
    Type(Name, ConnectToBadAddressIoDelete) \
        { nx::network::test::socketConnectToBadAddress(mkClient, true); } \
    Type(Name, ValidAfterPleaseStop) \
        { nx::network::test::socketIsValidAfterPleaseStop(mkClient); } \

#define NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, AcceptedSocketOptionsInheritance) \
        { nx::network::test::acceptedSocketOptionsInheritance(mkServer, mkClient); } \
    Type(Name, AcceptCancelIoSync) \
        { nx::network::test::socketAcceptCancelSync(mkServer, StopType::cancelIo); } \
    Type(Name, AcceptPleaseStopSync) \
        { nx::network::test::socketAcceptCancelSync(mkServer, StopType::pleaseStop); } \
    Type(Name, PleaseStopCancelsPostedCall) \
        { nx::network::test::serverSocketPleaseStopCancelsPostedCall(mkServer); } \

#define NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    Type(Name, TransferSyncAsyncSwitch) \
        { nx::network::test::socketSyncAsyncSwitch(mkServer, mkClient, true, endpointToConnectTo); } \
    Type(Name, TransferAsyncSyncSwitch) \
        { nx::network::test::socketSyncAsyncSwitch(mkServer, mkClient, false, endpointToConnectTo); } \
    Type(Name, TransferFragmentation) \
        { nx::network::test::socketTransferFragmentation(mkServer, mkClient, endpointToConnectTo); } \
    Type(Name, MultiConnect) \
        { nx::network::test::socketMultiConnect(mkServer, mkClient, endpointToConnectTo); } \

#define NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, std::nullopt)

#define NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE_EX(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo)

#define NX_NETWORK_CLIENT_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, std::nullopt) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, std::nullopt) \

#define NX_NETWORK_CLIENT_SOCKET_TEST_CASE_EX(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, endpointToConnectTo) \

#define NX_NETWORK_SERVER_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, std::nullopt) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, std::nullopt) \

#define NX_NETWORK_BOTH_SOCKET_TEST_CASE(Type, Name, mkServer, mkClient) \
    NX_NETWORK_SERVER_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, std::nullopt) \
    NX_NETWORK_CLIENT_SOCKET_TEST_GROUP(Type, Name, mkServer, mkClient, std::nullopt) \
    NX_NETWORK_TRANSFER_SOCKET_TESTS_GROUP(Type, Name, mkServer, mkClient, std::nullopt) \

