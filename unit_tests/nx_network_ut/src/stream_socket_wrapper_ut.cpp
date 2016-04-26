
#include <thread>

#include <gtest/gtest.h>

#include <utils/common/cpp14.h>
#include <utils/thread/sync_queue.h>

#include <nx/network/system_socket.h>
#include <nx/network/stream_socket_wrapper.h>


namespace nx {
namespace network {
namespace test {

static const SocketAddress kServerAddress("127.0.0.1:12345");
static const Buffer kTestMessage("hello world");
static const int kTestSize(kTestMessage.size());

class StreamSocketWrapperTest
    : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nx::TestSyncQueue<SystemError::ErrorCode> events;

        server = std::make_unique<TCPServerSocket>();
        ASSERT_TRUE(server->setReuseAddrFlag(true));
        ASSERT_TRUE(server->setNonBlockingMode(true));
        ASSERT_TRUE(server->bind(kServerAddress));
        ASSERT_TRUE(server->listen());
        server->acceptAsync([this, &events](
            SystemError::ErrorCode c, AbstractStreamSocket* s)
        {
            ASSERT_TRUE(s);
            ASSERT_TRUE(s->setNonBlockingMode(true));
            accepted.reset(s);

            events.push(c);
            readTestMessageAsync(accepted.get(), events);
        });

        client = std::make_unique<TCPSocket>();
        ASSERT_TRUE(client->setNonBlockingMode(true));
        client->connectAsync(kServerAddress, [this, &events](SystemError::ErrorCode c)
        {
            events.push(c);
            sendTestMessageAsync(client.get(), events);
        });

        for (size_t i = 0; i < 4; i++) // connect, accept, send, read
            ASSERT_EQ(events.pop(), SystemError::noError);
    }

    void TearDown() override
    {
        if (server) server->pleaseStopSync();
        if (client) client->pleaseStopSync();
        if (accepted) accepted->pleaseStopSync();
    }

    void sendTestMessageAsync(
        AbstractStreamSocket* socket, nx::TestSyncQueue<SystemError::ErrorCode>& events)
    {
        socket->sendAsync(kTestMessage, [&events](
            SystemError::ErrorCode c, size_t s)
        {
            EXPECT_EQ(s, kTestSize);
            events.push(c);
        });
    }

    void readTestMessageAsync(
        AbstractStreamSocket* socket, nx::TestSyncQueue<SystemError::ErrorCode>& events)
    {
        buffer.resize(0);
        buffer.reserve(kTestMessage.size() + 1);
        socket->readSomeAsync(&buffer, [this, &events](
            SystemError::ErrorCode c, size_t)
        {
            ASSERT_EQ(buffer, kTestMessage);
            events.push(c);
        });
    }

    Buffer buffer;
    std::unique_ptr<AbstractStreamServerSocket> server;
    std::unique_ptr<AbstractStreamSocket> client;
    std::unique_ptr<AbstractStreamSocket> accepted;
};

TEST_F(StreamSocketWrapperTest, Empty)
{
    nx::TestSyncQueue< SystemError::ErrorCode > events;
    readTestMessageAsync(client.get(), events);
    sendTestMessageAsync(accepted.get(), events);

    ASSERT_EQ(events.pop(), SystemError::noError); // send
    ASSERT_EQ(events.pop(), SystemError::noError); // read
}

TEST_F(StreamSocketWrapperTest, Async)
{
    nx::TestSyncQueue< SystemError::ErrorCode > events;

    client = std::make_unique<StreamSocketWrapper>(std::move(client));
    ASSERT_TRUE(client->setNonBlockingMode(true));
    readTestMessageAsync(client.get(), events);

    accepted = std::make_unique<StreamSocketWrapper>(std::move(accepted));
    ASSERT_TRUE(accepted->setNonBlockingMode(true));
    sendTestMessageAsync(accepted.get(), events);

    ASSERT_EQ(events.pop(), SystemError::noError); // send
    ASSERT_EQ(events.pop(), SystemError::noError); // read
}

TEST_F(StreamSocketWrapperTest, SyncAsync)
{
    nx::TestSyncQueue< SystemError::ErrorCode > events;

    client = std::make_unique<StreamSocketWrapper>(std::move(client));
    ASSERT_TRUE(client->setNonBlockingMode(true));
    readTestMessageAsync(client.get(), events);

    accepted = std::make_unique<StreamSocketWrapper>(std::move(accepted));
    ASSERT_TRUE(accepted->setNonBlockingMode(false));
    ASSERT_EQ(accepted->send(kTestMessage.data(), kTestSize), kTestSize);
    accepted.reset(); // shell be safe to do so

    ASSERT_EQ(events.pop(), SystemError::noError); // read
}

TEST_F(StreamSocketWrapperTest, Sync)
{
    std::thread clientThread([this]()
    {
        client = std::make_unique<StreamSocketWrapper>(std::move(client));
        buffer.resize(kTestSize + 1);
        ASSERT_EQ(client->recv(buffer.data(), buffer.size()), kTestSize);
        client.reset(); // shell be safe to do so

        buffer.resize(kTestSize);
        ASSERT_EQ(buffer, kTestMessage);
    });

    std::thread acceptedThread([this]()
    {
        accepted = std::make_unique<StreamSocketWrapper>(std::move(accepted));
        ASSERT_EQ(accepted->send(kTestMessage.data(), kTestSize), kTestSize);
        accepted.reset(); // shell be safe to do so
    });

    clientThread.join();
    acceptedThread.join();
}

} // namespace test
} // namespace network
} // namespace nx
