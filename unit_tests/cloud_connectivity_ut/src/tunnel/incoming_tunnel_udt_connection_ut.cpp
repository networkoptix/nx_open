
#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/udp_hole_punching/incoming_tunnel_udt_connection.h>
#include <nx/network/cloud/tunnel/udp_hole_punching/udp_hole_punching_acceptor.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <utils/thread/sync_queue.h>


namespace nx {
namespace network {
namespace cloud {
namespace test {

static const size_t kTestConnections = 10;
static const auto kConnectionId = QnUuid().createUuid().toSimpleString();
static const std::chrono::milliseconds kSocketTimeout(1000);
static const std::chrono::milliseconds kMaxKeepAliveInterval(3000);

class IncomingTunnelUdtConnectionTest
:
    public ::testing::Test
{
protected:
    void SetUp() override
    {
        TestSyncQueue<SystemError::ErrorCode> results;

        auto tmpSocket = makeSocket(true);
        NX_ASSERT(tmpSocket->setSendTimeout(0));
        NX_ASSERT(tmpSocket->setRecvTimeout(0));
        connectionAddress = tmpSocket->getLocalAddress();

        freeSocket = makeSocket(true);
        tmpSocket->connectAsync(freeSocket->getLocalAddress(), results.pusher());
        freeSocket->connectAsync(tmpSocket->getLocalAddress(), results.pusher());

        ASSERT_EQ(results.pop(), SystemError::noError);
        ASSERT_EQ(results.pop(), SystemError::noError);

        connection = std::make_unique<IncomingTunnelUdtConnection>(
            kConnectionId.toUtf8(),
            std::move(tmpSocket),
            kMaxKeepAliveInterval);
        acceptForever();
    }

    std::unique_ptr<UdtStreamSocket> makeSocket(bool randevous = false)
    {
        auto socket = std::make_unique<UdtStreamSocket>();
        NX_ASSERT(socket->setRendezvous(randevous));
        NX_ASSERT(socket->setSendTimeout(kSocketTimeout.count()));
        NX_ASSERT(socket->setRecvTimeout(kSocketTimeout.count()));
        NX_ASSERT(socket->setNonBlockingMode(true));
        NX_ASSERT(socket->bind(SocketAddress(HostAddress::localhost, 0)));
        return std::move(socket);
    }

    void acceptForever()
    {
        connection->accept(
            [this](
                SystemError::ErrorCode code,
                std::unique_ptr<AbstractStreamSocket> socket)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    acceptedSockets.push_back(std::move(socket));
                }

                acceptResults.push(code);
                if (code == SystemError::noError)
                    acceptForever();
            });
    }

    void runConnectingSockets(size_t count = 1)
    {
        if (!count)
            return;

        auto socket = makeSocket();
        socket->connectAsync(
            connectionAddress,
            [this, count](SystemError::ErrorCode code)
            {
                NX_LOGX(lm("Connect result: %1")
                    .arg(SystemError::toString(code)), cl_logDEBUG2);

                connectResults.push(code);
                if (code == SystemError::noError)
                    runConnectingSockets(count - 1);
            });

        QnMutexLocker lock(&m_mutex);
        connectSockets.push_back(std::move(socket));
    }

    void TearDown() override
    {
        if (connection)
            connection->pleaseStopSync();

        if (freeSocket)
            freeSocket->pleaseStopSync();

        for (auto& socket : connectSockets)
            socket->pleaseStopSync();
    }

    SocketAddress connectionAddress;
    std::unique_ptr<IncomingTunnelUdtConnection> connection;
    TestSyncQueue<SystemError::ErrorCode> acceptResults;

    std::unique_ptr<UdtStreamSocket> freeSocket;
    TestSyncQueue<SystemError::ErrorCode> connectResults;

    QnMutex m_mutex;
    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedSockets;
    std::vector<std::unique_ptr<AbstractStreamSocket>> connectSockets;
};

TEST_F(IncomingTunnelUdtConnectionTest, Timeout)
{
    ASSERT_EQ(acceptResults.pop(), SystemError::timedOut);
}

TEST_F(IncomingTunnelUdtConnectionTest, Connections)
{
    runConnectingSockets(kTestConnections);
    for (size_t i = 0; i < kTestConnections; ++i)
    {
        ASSERT_EQ(connectResults.pop(), SystemError::noError) << "i = " << i;
        ASSERT_EQ(acceptResults.pop(), SystemError::noError) << "i = " << i;
    }

    EXPECT_TRUE(connectResults.isEmpty());
    EXPECT_TRUE(acceptResults.isEmpty());

    // then connection closes by timeout
    ASSERT_EQ(acceptResults.pop(), SystemError::timedOut);
}

TEST_F(IncomingTunnelUdtConnectionTest, SynAck)
{
    // we can connect right after start
    runConnectingSockets();
    ASSERT_EQ(connectResults.pop(), SystemError::noError);
    ASSERT_EQ(acceptResults.pop(), SystemError::noError);

    {
        hpm::api::UdpHolePunchingSyn syn;
        stun::Message request;
        syn.serialize(&request);

        Buffer buffer;
        buffer.reserve(1000);
        stun::MessageSerializer serializer;
        size_t processed;
        serializer.setMessage(&request);
        serializer.serialize(&buffer, &processed);

        std::promise<void> promise;
        freeSocket->sendAsync(
            buffer,
            [this, &buffer, &promise](SystemError::ErrorCode code, size_t size)
            {
                ASSERT_EQ(code, SystemError::noError);
                ASSERT_EQ(size, buffer.size());

                buffer.resize(0);
                freeSocket->readSomeAsync(
                    &buffer,
                    [&buffer, &promise](SystemError::ErrorCode code, size_t)
                    {
                        ASSERT_EQ(code, SystemError::noError);

                        stun::Message response;
                        stun::MessageParser parser;
                        parser.setMessage(&response);

                        size_t processed;
                        ASSERT_EQ(parser.parse(buffer, &processed),
                                  nx_api::ParserState::done);

                        hpm::api::UdpHolePunchingSynAck synAck;
                        ASSERT_TRUE(synAck.parse(response));
                        ASSERT_EQ(synAck.connectSessionId, kConnectionId.toUtf8());

                        promise.set_value();
                    });
            });

        promise.get_future().wait();
    }

    // we still can connect
    runConnectingSockets();
    ASSERT_EQ(connectResults.pop(), SystemError::noError);
    ASSERT_EQ(acceptResults.pop(), SystemError::noError);

    {
        Buffer buffer("someTrash");
        std::promise<void> promise;
        freeSocket->sendAsync(
            buffer,
            [this, &buffer, &promise](SystemError::ErrorCode code, size_t size)
            {
                ASSERT_EQ(code, SystemError::noError);
                ASSERT_EQ(size, buffer.size());

                buffer.resize(0);
                buffer.reserve(1);
                freeSocket->readSomeAsync(
                    &buffer,
                    [&buffer, &promise](SystemError::ErrorCode code, size_t)
                    {
                        ASSERT_NE(code, SystemError::noError);
                        promise.set_value();
                    });
            });

        promise.get_future().wait();
    }

    // connection was brocken by wrong packet
    runConnectingSockets();
    ASSERT_NE(connectResults.pop(), SystemError::noError);
    ASSERT_EQ(acceptResults.pop(), SystemError::invalidData);
}

TEST_F(IncomingTunnelUdtConnectionTest, PleaseStop)
{
}

TEST_F(IncomingTunnelUdtConnectionTest, PleaseStopOnRun)
{
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kTestConnections; ++i)
    {
        threads.push_back(std::thread(
            [this]()
            {
                for (;;)
                {
                    auto socket = std::make_unique<UdtStreamSocket>();
                    NX_ASSERT(socket->setSendTimeout(kSocketTimeout.count()));
                    if (!socket->connect(connectionAddress))
                        return;
                }
            }));
    }

    std::promise<void> promise;
    connection->pleaseStop(
        [this, &promise]()
        {
            connection.reset();
            promise.set_value();
        });

    promise.get_future().wait();
    for (auto& thread : threads)
        thread.join();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
