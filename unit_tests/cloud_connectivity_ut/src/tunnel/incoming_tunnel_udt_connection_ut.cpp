#include <gtest/gtest.h>

#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/cloud/tunnel/udp/acceptor.h>
#include <nx/network/cloud/tunnel/udp/incoming_tunnel_connection.h>
#include <nx/network/stun/message.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/sync_queue.h>

#include <nx/utils/scope_guard.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {
namespace test {

static const size_t kTestConnections = 10;
static const auto kConnectionId = QnUuid().createUuid().toSimpleString();
static const std::chrono::milliseconds kSocketTimeout(3000);
static const std::chrono::milliseconds kMaxKeepAliveInterval(3000);

class UdpIncomingTunnelConnectionTest:
    public ::testing::Test
{
protected:
    void SetUp() override
    {
        utils::TestSyncQueue<SystemError::ErrorCode> results;

        auto tmpSocket = makeSocket(true);
        auto tmpSocketGuard = makeScopeGuard([&tmpSocket]() { tmpSocket->pleaseStopSync(); });

        ASSERT_TRUE(tmpSocket->setSendTimeout(0));
        ASSERT_TRUE(tmpSocket->setRecvTimeout(0));
        m_connectionAddress = tmpSocket->getLocalAddress();

        m_freeSocket = makeSocket(true);
        tmpSocket->connectAsync(m_freeSocket->getLocalAddress(), results.pusher());
        m_freeSocket->connectAsync(tmpSocket->getLocalAddress(), results.pusher());

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
                tmpSocketGuard.disarm();

                cc->start(nullptr /* do not wait for select in test */);
                m_connection = std::make_unique<udp::IncomingTunnelConnection>(std::move(cc));

                acceptForever();
                startedPromise.set_value();
            });

        startedPromise.get_future().wait();
    }

    void TearDown() override
    {
        if (const auto connection = takeConnection())
            connection->pleaseStopSync();

        m_freeSocket->pleaseStopSync();
        for (auto& socket : m_connectSockets)
            socket->pleaseStopSync();
    }

    std::unique_ptr<UdtStreamSocket> makeSocket(bool randevous = false)
    {
        auto socket = std::make_unique<UdtStreamSocket>(AF_INET);
        NX_CRITICAL(socket->setRendezvous(randevous));
        NX_CRITICAL(socket->setSendTimeout(kSocketTimeout.count()));
        NX_CRITICAL(socket->setRecvTimeout(kSocketTimeout.count()));
        NX_CRITICAL(socket->setNonBlockingMode(true));
        NX_CRITICAL(socket->bind(nx::network::SocketAddress(nx::network::HostAddress::localhost, 0)));
        return std::move(socket);
    }

    void acceptForever()
    {
        m_connection->accept(
            [this](
                SystemError::ErrorCode code,
                std::unique_ptr<nx::network::AbstractStreamSocket> socket)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    m_acceptedSockets.push_back(std::move(socket));
                    m_acceptResults.push(code);

                    if (code == SystemError::noError && m_connection)
                        return acceptForever();
                }

                takeConnection().reset();
            });
    }

    void runConnectingSockets(size_t count = 1)
    {
        if (!count)
            return;

        auto socket = makeSocket();
        socket->connectAsync(
            m_connectionAddress,
            [this, count](SystemError::ErrorCode code)
            {
                NX_LOGX(lm("Connect result: %1")
                    .arg(SystemError::toString(code)), cl_logDEBUG2);

                m_connectResults.push(code);
                if (code == SystemError::noError)
                    runConnectingSockets(count - 1);
            });

        QnMutexLocker lock(&m_mutex);
        m_connectSockets.push_back(std::move(socket));
    }

    std::unique_ptr<udp::IncomingTunnelConnection> takeConnection()
    {
        QnMutexLocker lock(&m_mutex);
        auto localConection = std::move(m_connection);
        m_connection = nullptr;
        return localConection;
    }

    QnMutex m_mutex;
    nx::network::SocketAddress m_connectionAddress;
    std::unique_ptr<udp::IncomingTunnelConnection> m_connection;
    std::unique_ptr<UdtStreamSocket> m_freeSocket;
    utils::TestSyncQueue<SystemError::ErrorCode> m_acceptResults;
    utils::TestSyncQueue<SystemError::ErrorCode> m_connectResults;
    std::vector<std::unique_ptr<nx::network::AbstractStreamSocket>> m_acceptedSockets;
    std::vector<std::unique_ptr<nx::network::AbstractStreamSocket>> m_connectSockets;
};

TEST_F(UdpIncomingTunnelConnectionTest, Timeout)
{
    ASSERT_EQ(m_acceptResults.pop(), SystemError::timedOut);
}

TEST_F(UdpIncomingTunnelConnectionTest, Connections)
{
    runConnectingSockets(kTestConnections);
    for (size_t i = 0; i < kTestConnections; ++i)
    {
        ASSERT_EQ(m_connectResults.pop(), SystemError::noError) << "i = " << i;
        ASSERT_EQ(m_acceptResults.pop(), SystemError::noError) << "i = " << i;
    }

    EXPECT_TRUE(m_connectResults.isEmpty());
    EXPECT_TRUE(m_acceptResults.isEmpty());

    // then connection closes by timeout
    ASSERT_EQ(m_acceptResults.pop(), SystemError::timedOut);
}

TEST_F(UdpIncomingTunnelConnectionTest, SynAck)
{
    // we can connect right after start
    runConnectingSockets();
    ASSERT_EQ(m_connectResults.pop(), SystemError::noError);
    ASSERT_EQ(m_acceptResults.pop(), SystemError::noError);

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
        m_freeSocket->sendAsync(
            buffer,
            [this, &buffer, &promise](SystemError::ErrorCode code, size_t size)
            {
                ASSERT_EQ(code, SystemError::noError);
                ASSERT_EQ(size, (size_t)buffer.size());

                buffer.resize(0);
                m_freeSocket->readSomeAsync(
                    &buffer,
                    [&buffer, &promise](SystemError::ErrorCode code, size_t)
                    {
                        ASSERT_EQ(code, SystemError::noError);

                        stun::Message response;
                        stun::MessageParser parser;
                        parser.setMessage(&response);

                        size_t processed;
                        ASSERT_EQ(parser.parse(buffer, &processed),
                                  nx::network::server::ParserState::done);
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
    ASSERT_EQ(m_connectResults.pop(), SystemError::noError);
    ASSERT_EQ(m_acceptResults.pop(), SystemError::noError);

    {
        Buffer buffer("someTrash");
        nx::utils::promise<void> promise;
        m_freeSocket->sendAsync(
            buffer,
            [this, &buffer, &promise](SystemError::ErrorCode code, size_t size)
            {
                ASSERT_EQ(code, SystemError::noError);
                ASSERT_EQ(size, (size_t)buffer.size());

                buffer.resize(0);
                buffer.reserve(1);
                m_freeSocket->readSomeAsync(
                    &buffer,
                    [&buffer, &promise](SystemError::ErrorCode code, size_t size)
                    {
                        ASSERT_TRUE(code != SystemError::noError || size == 0);
                        promise.set_value();
                    });
            });

        promise.get_future().wait();
    }

    // connection was broken by wrong packet
    runConnectingSockets();
    ASSERT_NE(m_connectResults.pop(), SystemError::noError);
    ASSERT_EQ(m_acceptResults.pop(), SystemError::invalidData);
}

TEST_F(UdpIncomingTunnelConnectionTest, PleaseStop)
{
    // Tests if TearDown() works fine right after SetUp()
}

TEST_F(UdpIncomingTunnelConnectionTest, PleaseStopOnRun)
{
    std::vector<nx::utils::thread> threads;
    for (size_t i = 0; i < kTestConnections; ++i)
    {
        threads.push_back(nx::utils::thread(
            [this]()
            {
                for (;;)
                {
                    auto socket = std::make_unique<UdtStreamSocket>(AF_INET);
                    ASSERT_TRUE(socket->setSendTimeout(kSocketTimeout.count()));
                    if (!socket->connect(m_connectionAddress, nx::network::deprecated::kDefaultConnectTimeout))
                        return;
                }
            }));
    }

    if (const auto connection = takeConnection())
        connection->pleaseStopSync();

    for (auto& thread: threads)
        thread.join();
}

//-------------------------------------------------------------------------------------------------

namespace {

class ServerSocketStub:
    public TCPServerSocket
{
public:
    virtual void acceptAsync(AcceptCompletionHandler handler) override
    {
        post(
            [handler = std::move(handler)]()
            {
                handler(SystemError::connectionReset, nullptr);
            });
    }
};

} // namespace

class UdpIncomingTunnelConnection:
    public ::testing::Test
{
public:
    ~UdpIncomingTunnelConnection()
    {
        if (m_tunnelConnection)
            m_tunnelConnection->pleaseStopSync();
    }

protected:
    void givenConnectionWithBrokenUnderlyingSocket()
    {
        auto controlConnection = createControlConnection();
        auto controlConnectionSocketPtr = controlConnection->socket();
        nx::utils::promise<void> created;
        controlConnectionSocketPtr->post(
            [this, &created, controlConnection = std::move(controlConnection)]() mutable
            {
                m_tunnelConnection = std::make_unique<udp::IncomingTunnelConnection>(
                    std::move(controlConnection),
                    std::make_unique<ServerSocketStub>());
                created.set_value();
            });
        created.get_future().wait();
    }

    void whenInvokeAccept()
    {
        using namespace std::placeholders;

        m_tunnelConnection->accept(
            std::bind(&UdpIncomingTunnelConnection::saveAcceptResult, this, _1, _2));
    }

    void thenErrorIsReported()
    {
        const auto result = m_acceptResults.pop();
        ASSERT_NE(SystemError::noError, std::get<0>(result));
        ASSERT_EQ(nullptr, std::get<1>(result));
    }

private:
    using AcceptResult = std::tuple<SystemError::ErrorCode, std::unique_ptr<nx::network::AbstractStreamSocket>>;

    nx::utils::SyncQueue<AcceptResult> m_acceptResults;
    std::unique_ptr<udp::IncomingTunnelConnection> m_tunnelConnection;

    std::unique_ptr<IncomingControlConnection> createControlConnection()
    {
        return std::make_unique<IncomingControlConnection>(
            "connectionId",
            std::make_unique<UdtStreamSocket>(SocketFactory::udpIpVersion()),
            nx::hpm::api::ConnectionParameters());
        return nullptr;
    }

    void saveAcceptResult(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<nx::network::AbstractStreamSocket> connection)
    {
        m_acceptResults.push(std::make_tuple(sysErrorCode, std::move(connection)));
    }
};

TEST_F(UdpIncomingTunnelConnection, forwards_error_code_from_underlying_socket)
{
    givenConnectionWithBrokenUnderlyingSocket();
    whenInvokeAccept();
    thenErrorIsReported();
}

} // namespace test
} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
