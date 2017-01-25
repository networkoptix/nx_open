
#include <thread>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace stun {
namespace test {

class TestServer:
    public SocketServer
{
public:
    TestServer(const nx::stun::MessageDispatcher& dispatcher):
        SocketServer(&dispatcher, false)
    {
    }

    virtual ~TestServer() override
    {
        pleaseStopSync();
        for (auto& connection: connections)
        {
            connection->pleaseStopSync();
            connection.reset();
        }
    }

    std::vector<std::shared_ptr<ServerConnection>> connections;

protected:
    virtual std::shared_ptr<ServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        auto connection = SocketServer::createConnection(std::move(_socket));
        connections.push_back(connection);
        return connection;
    };
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
        settings.reconnectPolicy.setInitialDelay(std::chrono::seconds(3));
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
        const auto connection = server->connections.front();
        connection->sendMessage(
            Message(Header(MessageClass::indication, method)),
            sendWaiter.pusher());

        return sendWaiter.pop();
    }

    nx::stun::MessageDispatcher dispatcher;
    std::shared_ptr<AbstractAsyncClient> client;
    std::unique_ptr<TestServer> server;
};

TEST_F(StunClientServerTest, Connectivity)
{
    EXPECT_EQ(sendTestRequestSync(), SystemError::notConnected); // no address

    const auto address = startServer();
    server.reset();
    client->connect(address);
    EXPECT_THAT(sendTestRequestSync(), testing::AnyOf(
        SystemError::connectionRefused, SystemError::connectionReset,
        SystemError::timedOut)); // no server to connect

    startServer(address);
    EXPECT_EQ(sendTestRequestSync(), SystemError::noError); // ok
    EXPECT_EQ(1, server->connectionCount());

    server.reset();
    EXPECT_NE(sendTestRequestSync(), SystemError::noError); // no server

    utils::promise<void> promise;
    client->addOnReconnectedHandler([&]{ promise.set_value(); });

    startServer(address);
    promise.get_future().wait(); // automatic reconnect is expected

    // there might be a small delay before server creates connection from
    // accepted socket
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(1, server->connectionCount());
}

TEST_F(StunClientServerTest, RequestResponse)
{
    const auto t1 = std::chrono::steady_clock::now();

    const auto address = startServer();
    client->connect(address);
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
        ASSERT_EQ(ntohl(HostAddress::localhost.ipV4()->s_addr), addr->address.ipv4);
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
    client->connect(address);
    client->setIndicationHandler(0xAB, recvWaiter.pusher());
    client->setIndicationHandler(0xCD, recvWaiter.pusher());

    EXPECT_EQ(sendTestRequestSync(), SystemError::noError);
    EXPECT_EQ(1, server->connectionCount());

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
    client->connect(address);
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

} // namespace test
} // namespace hpm
} // namespase nx
