
#include <gtest/gtest.h>

#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/cloud/tunnel/udp/acceptor.h>
#include <nx/network/cloud/tunnel/udp/incoming_tunnel_connection.h>
#include <nx/network/stun/message.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <utils/thread/sync_queue.h>


namespace nx {
namespace network {
namespace cloud {
namespace udp {
namespace test {

static const size_t kTestConnections = 10;
static const auto kConnectionId = QnUuid().createUuid().toSimpleString();
static const std::chrono::milliseconds kSocketTimeout(1000);
static const std::chrono::milliseconds kMaxKeepAliveInterval(3000);

class IncomingTunnelConnectionTest
:
    public ::testing::Test
{
protected:
    void SetUp() override
    {
        TestSyncQueue<SystemError::ErrorCode> results;

        auto tmpSocket = makeSocket(true);
        ASSERT_TRUE(tmpSocket->setSendTimeout(0));
        ASSERT_TRUE(tmpSocket->setRecvTimeout(0));
        connectionAddress = tmpSocket->getLocalAddress();

        freeSocket = makeSocket(true);
        tmpSocket->connectAsync(freeSocket->getLocalAddress(), results.pusher());
        freeSocket->connectAsync(tmpSocket->getLocalAddress(), results.pusher());

        ASSERT_EQ(results.pop(), SystemError::noError);
        ASSERT_EQ(results.pop(), SystemError::noError);

        nx::hpm::api::ConnectionParameters connectionParameters;
        connectionParameters.udpTunnelKeepAliveInterval = kMaxKeepAliveInterval;
        connectionParameters.udpTunnelKeepAliveRetries = 1;

        utils::promise<void> startedPromise;
        tmpSocket->dispatch(
            [&]()
            {
                auto cc = std::make_unique<IncomingControlConnection>(
                    kConnectionId.toUtf8(), std::move(tmpSocket), connectionParameters);

                cc->start(nullptr /* do not wait for select in test */);
                connection = std::make_unique<IncomingTunnelConnection>(std::move(cc));
                
                acceptForever();
                startedPromise.set_value();
            });
            
        startedPromise.get_future().wait();
    }

    std::unique_ptr<UdtStreamSocket> makeSocket(bool randevous = false)
    {
        auto socket = std::make_unique<UdtStreamSocket>();
        NX_CRITICAL(socket->setRendezvous(randevous));
        NX_CRITICAL(socket->setSendTimeout(kSocketTimeout.count()));
        NX_CRITICAL(socket->setRecvTimeout(kSocketTimeout.count()));
        NX_CRITICAL(socket->setNonBlockingMode(true));
        NX_CRITICAL(socket->bind(SocketAddress(HostAddress::localhost, 0)));
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
                else
                    connection.reset();
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
    std::unique_ptr<IncomingTunnelConnection> connection;
    TestSyncQueue<SystemError::ErrorCode> acceptResults;

    std::unique_ptr<UdtStreamSocket> freeSocket;
    TestSyncQueue<SystemError::ErrorCode> connectResults;

    QnMutex m_mutex;
    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedSockets;
    std::vector<std::unique_ptr<AbstractStreamSocket>> connectSockets;
};

TEST_F(IncomingTunnelConnectionTest, Timeout)
{
    ASSERT_EQ(acceptResults.pop(), SystemError::timedOut);
}

TEST_F(IncomingTunnelConnectionTest, Connections)
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

TEST_F(IncomingTunnelConnectionTest, SynAck)
{
    // we can connect right after start
    runConnectingSockets();
    ASSERT_EQ(connectResults.pop(), SystemError::noError);
    ASSERT_EQ(acceptResults.pop(), SystemError::noError);

    {
        hpm::api::UdpHolePunchingSynRequest syn;
        stun::Message request;
        syn.serialize(&request);

        Buffer buffer;
        buffer.reserve(1000);
        stun::MessageSerializer serializer;
        size_t processed;
        serializer.setMessage(&request);
        serializer.serialize(&buffer, &processed);

        nx::utils::promise<void> promise;
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
                        ASSERT_EQ(response.header.messageClass,
                                  stun::MessageClass::successResponse);
                        ASSERT_EQ(response.header.method,
                                  hpm::api::UdpHolePunchingSynResponse::kMethod);

                        hpm::api::UdpHolePunchingSynResponse synAck;
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
        nx::utils::promise<void> promise;
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
                    [&buffer, &promise](SystemError::ErrorCode code, size_t size)
                    {
                        ASSERT_TRUE(code != SystemError::noError || size == 0);
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

TEST_F(IncomingTunnelConnectionTest, PleaseStop)
{
}

TEST_F(IncomingTunnelConnectionTest, PleaseStopOnRun)
{
    std::vector<nx::utils::thread> threads;
    for (size_t i = 0; i < kTestConnections; ++i)
    {
        threads.push_back(nx::utils::thread(
            [this]()
            {
                for (;;)
                {
                    auto socket = std::make_unique<UdtStreamSocket>();
                    ASSERT_TRUE(socket->setSendTimeout(kSocketTimeout.count()));
                    if (!socket->connect(connectionAddress))
                        return;
                }
            }));
    }

    nx::utils::promise<void> promise;
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
} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
