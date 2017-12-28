#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/future.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/test_support/sync_queue.h>

namespace nx {
namespace stun {
namespace test {

class TestServer:
    public SocketServer
{
public:
    TestServer(const nx::network::stun::MessageDispatcher& dispatcher):
        SocketServer(&dispatcher, false),
        m_totalConnectionsAccepted(0)
    {
    }

    virtual ~TestServer() override
    {
        pleaseStopSync();
        for (auto& connection: m_connections)
        {
            connection->pleaseStopSync();
            connection.reset();
        }
    }

    std::size_t totalConnectionsAccepted() const
    {
        return m_totalConnectionsAccepted.load();
    }

    std::vector<std::shared_ptr<ServerConnection>>& connections()
    {
        return m_connections;
    }

protected:
    virtual std::shared_ptr<ServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        ++m_totalConnectionsAccepted;
        auto connection = SocketServer::createConnection(std::move(_socket));
        m_connections.push_back(connection);
        return connection;
    };

private:
    std::atomic<std::size_t> m_totalConnectionsAccepted;
    std::vector<std::shared_ptr<ServerConnection>> m_connections;
};

class StunClientServerTest:
    public ::testing::Test
{
protected:
    static AbstractAsyncClient::Settings defaultSettings()
    {
        AbstractAsyncClient::Settings settings;
        settings.sendTimeout = std::chrono::seconds(1);
        settings.recvTimeout = std::chrono::seconds(1);

        settings.reconnectPolicy.delayMultiplier = 2;
        settings.reconnectPolicy.initialDelay = std::chrono::milliseconds(500);
        settings.reconnectPolicy.maxDelay = std::chrono::seconds(5);
        settings.reconnectPolicy.maxRetryCount = network::RetryPolicy::kInfiniteRetries;
        return settings;
    }

    StunClientServerTest():
        client(std::make_shared<AsyncClient>(defaultSettings()))
    {
    }

    ~StunClientServerTest()
    {
        if (client)
            client->pleaseStopSync();
        if (server)
            server->pleaseStopSync();
    }

    SystemError::ErrorCode sendTestRequestSync()
    {
        Message request(Header(MessageClass::request, MethodType::bindingMethod));
        utils::TestSyncMultiQueue<SystemError::ErrorCode, Message> waiter;
        client->sendRequest(std::move(request), waiter.pusher());
        return waiter.pop().first;
    }

    SocketAddress startServer(
        const SocketAddress& address = SocketAddress(HostAddress::localhost, 0))
    {
        server = std::make_unique<TestServer>(dispatcher);

        EXPECT_TRUE(server->bind(address)) <<
            SystemError::getLastOSErrorText().toStdString();

        EXPECT_TRUE(server->listen()) <<
            SystemError::getLastOSErrorText().toStdString();

        return server->address();
    }

    SystemError::ErrorCode sendIndicationSync(int method)
    {
        utils::TestSyncQueue<SystemError::ErrorCode> sendWaiter;
        const auto connection = server->connections().front();
        connection->sendMessage(
            Message(Header(MessageClass::indication, method)),
            sendWaiter.pusher());

        return sendWaiter.pop();
    }

    nx::network::stun::MessageDispatcher dispatcher;
    std::shared_ptr<AbstractAsyncClient> client;
    std::unique_ptr<TestServer> server;
};

TEST_F(StunClientServerTest, Connectivity)
{
    std::atomic<size_t> timerTicks;
    const auto incrementTimer = [&timerTicks]() { ++timerTicks; };
    const auto timerPeriod = defaultSettings().reconnectPolicy.initialDelay / 2;
    utils::TestSyncQueue<bool> reconnectEvents;
    auto reconnectHandler =
        [&reconnectEvents] { reconnectEvents.push(true); };

    EXPECT_EQ(SystemError::notConnected, sendTestRequestSync()); //< No address.

    const auto address = startServer();
    server.reset();
    client->connect(nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(address));
    auto clientGuard = makeScopeGuard([this]() { client->pleaseStopSync(); });

    EXPECT_THAT(sendTestRequestSync(), testing::AnyOf(
        SystemError::connectionRefused, SystemError::connectionReset,
        SystemError::timedOut)); //< No server to connect.

    startServer(address);
    ASSERT_EQ(SystemError::noError, sendTestRequestSync());
    ASSERT_EQ(1U, server->connectionCount());

    server.reset();
    ASSERT_NE(SystemError::noError, sendTestRequestSync());

    client->addOnReconnectedHandler(reconnectHandler);

    startServer(address);
    reconnectEvents.pop(); //< Automatic reconnect is expected.

    // There might be a small delay before server creates connection from accepted socket.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(1U, server->connectionCount());

    ASSERT_TRUE(client->addConnectionTimer(timerPeriod, incrementTimer, nullptr));
    // Checking that timer ticks certain times per some period is not reliable.
    while (timerTicks < 3)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    server.reset();
    ASSERT_NE(sendTestRequestSync(), SystemError::noError);
    timerTicks = 0;

    // Wait some time so client will retry to connect again and again.
    std::this_thread::sleep_for(defaultSettings().reconnectPolicy.initialDelay * 5);
    ASSERT_EQ(0U, timerTicks); //< Timer does not tick while connection is broken;

    startServer(address);
    reconnectEvents.pop(); // Automatic reconnect is expected, again.

    while (server->connectionCount() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(1U, server->connectionCount()) <<
        "Total connections accepted: " << server->totalConnectionsAccepted();

    ASSERT_TRUE(client->addConnectionTimer(timerPeriod, incrementTimer, nullptr)) <<
        "Server connection count " << server->connectionCount();
    // Checking that timer ticks certain times per some period is not reliable.
    while (timerTicks < 3)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(StunClientServerTest, RequestResponse)
{
    const auto t1 = std::chrono::steady_clock::now();

    const auto address = startServer();
    client->connect(nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(address));
    {
        Message request(Header(MessageClass::request, MethodType::bindingMethod));

        utils::TestSyncMultiQueue<SystemError::ErrorCode, Message> waiter;
        client->sendRequest(std::move(request), waiter.pusher());

        const auto result = waiter.pop();
        const auto timePassed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t1);

        ASSERT_EQ(SystemError::noError, result.first)
            << "after: " << timePassed.count() << " ms";

        const auto& response = result.second;
        ASSERT_EQ(MessageClass::successResponse, response.header.messageClass);
        ASSERT_EQ(MethodType::bindingMethod, response.header.method);

        const auto addr = response.getAttribute<stun::attrs::XorMappedAddress>();
        ASSERT_NE(nullptr, addr);
        ASSERT_EQ(stun::attrs::XorMappedAddress::IPV4, addr->family);
        const auto ipv4 = ntohl(HostAddress::localhost.ipV4()->s_addr);
        ASSERT_EQ(ipv4, addr->address.ipv4);
    }
    {
        Message request(Header(MessageClass::request, 0xFFF /* unknown */));

        utils::TestSyncMultiQueue<SystemError::ErrorCode, Message> waiter;
        client->sendRequest(std::move(request), waiter.pusher());

        const auto result = waiter.pop();
        ASSERT_EQ(result.first, SystemError::noError);

        const auto& response = result.second;
        ASSERT_EQ(response.header.messageClass, MessageClass::errorResponse);
        ASSERT_EQ(response.header.method, 0xFFF);

        const auto error = response.getAttribute<stun::attrs::ErrorCode>();
        ASSERT_NE(error, nullptr);
        ASSERT_EQ(error->getCode(), 404);
        ASSERT_EQ(error->getString(), String("Method is not supported"));
    }
}

TEST_F(StunClientServerTest, Indications)
{
    const auto address = startServer();

    utils::TestSyncQueue<Message> recvWaiter;
    client->connect(nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(address));
    client->setIndicationHandler(0xAB, recvWaiter.pusher());
    client->setIndicationHandler(0xCD, recvWaiter.pusher());

    EXPECT_EQ(sendTestRequestSync(), SystemError::noError);
    while (server->connectionCount() < 1)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    EXPECT_EQ(sendIndicationSync(0xAB), SystemError::noError);
    EXPECT_EQ(sendIndicationSync(0xCD), SystemError::noError);
    EXPECT_EQ(sendIndicationSync(0xAD), SystemError::noError);

    EXPECT_EQ(recvWaiter.pop().header.method, 0xAB);
    EXPECT_EQ(recvWaiter.pop().header.method, 0xCD);
    EXPECT_TRUE(recvWaiter.isEmpty()); // 3rd indication is not subscribed
}

class TestUser:
    public AsyncClientUser
{
public:
    TestUser(std::shared_ptr<AbstractAsyncClient> client):
        AsyncClientUser(std::move(client))
    {
    }

    void request()
    {
        Message request(Header(MessageClass::request, MethodType::bindingMethod));
        sendRequest(std::move(request), responses.pusher());
    }

    utils::TestSyncMultiQueue<SystemError::ErrorCode, Message> responses;
};

static const size_t USER_COUNT = 3;
static const size_t REQUEST_COUNT = 10;
static const size_t REQUEST_RATIO = 5;

TEST_F(StunClientServerTest, AsyncClientUser)
{
    const auto address = startServer();
    client->connect(nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(address));
    for (size_t uc = 0; uc <USER_COUNT; ++uc)
    {
        auto user = std::make_shared<TestUser>(client);
        for (size_t rc = 0; rc <REQUEST_COUNT * REQUEST_RATIO; ++rc)
            user->request();
        for (size_t rc = 0; rc <REQUEST_COUNT; ++rc) // only part is waited
            EXPECT_EQ(user->responses.pop().first, SystemError::noError);
        user->pleaseStopSync();
    }
}

TEST_F(StunClientServerTest, cancellation)
{
    const auto address = startServer();

    utils::TestSyncQueue<bool> reconnectEvents;
    auto reconnectHandler =
        [&reconnectEvents] { reconnectEvents.push(true); };

    std::atomic<size_t> timerTicks(0);
    const auto incrementTimer = [&timerTicks]() { ++timerTicks; };
    const auto timerPeriod = defaultSettings().reconnectPolicy.initialDelay / 2;

    auto clientGuard = makeScopeGuard([this]() { client->pleaseStopSync(); });
    client->connect(nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(address));
    client->addOnReconnectedHandler(reconnectHandler);
    client->addConnectionTimer(timerPeriod, incrementTimer, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));

    server.reset();
}

} // namespace test
} // namespace hpm
} // namespace nx
