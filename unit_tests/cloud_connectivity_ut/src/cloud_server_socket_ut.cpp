#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/test_support/stun_async_client_mock.h>

namespace nx {
namespace network {
namespace cloud {

/** Accepts usual TCP connections */
struct FakeTcpTunnelConnection : AbstractTunnelConnection
{
    FakeTcpTunnelConnection(
        String remotePeerId, SocketAddress address, size_t sockets = 1000)
    :
        AbstractTunnelConnection(std::move(remotePeerId)),
        m_sockets(sockets),
        m_server(std::make_unique<TCPServerSocket>())
    {
        assert(m_server->setNonBlockingMode(true));
        assert(m_server->setReuseAddrFlag(true));
        if (!m_server->bind(address) || !m_server->listen())
            m_sockets = 0;

        NX_LOGX(lm("for %1 listen %2 for %3 sockets")
                .arg(m_remotePeerId).arg(address.toString())
                .arg(m_sockets), cl_logDEBUG1);
    }

    void establishNewConnection(
        boost::optional<std::chrono::milliseconds>,
        SocketAttributes, SocketHandler) override
    {
        // not supposed to be used for connect
        assert(false);
    }

    void accept(SocketHandler handler) override
    {
        if (m_sockets == 0)
            return m_server->post([handler]()
            {
                handler(SystemError::connectionReset, nullptr, false);
            });

        m_server->acceptAsync([this, handler](
            SystemError::ErrorCode c, AbstractStreamSocket* s)
        {
            EXPECT_EQ(c, SystemError::noError);
            --m_sockets;
            handler(c, std::unique_ptr<AbstractStreamSocket>(s), m_sockets);
        });
    }

    void pleaseStop(std::function<void()> handler) override
    {
        m_server->pleaseStop([this, handler]
        {
            NX_LOGX(lm("exhausted"), cl_logDEBUG1);
            m_server.reset();
            handler();
        });
    }

    size_t m_sockets;
    std::unique_ptr<AbstractStreamServerSocket> m_server;
};

/** Creates several \class FakeTcpTunnelConnection */
struct FakeTcpTunnelAcceptor : public AbstractTunnelAcceptor
{
    FakeTcpTunnelAcceptor(
        String remotePeerId, boost::optional<SocketAddress> address,
        size_t socketsPerConnection = 1000)
    :
        m_remotePeerId(std::move(remotePeerId)),
        m_address(std::move(address)),
        m_socketsPerConnection(socketsPerConnection),
        m_ioThreadSocket(new TCPSocket)
    {
    }

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractTunnelConnection>)> handler) override
    {
        if (!m_address)
            return m_ioThreadSocket->registerTimer(
                100, [handler](){ handler(SystemError::timedOut, nullptr); });

        auto connection = std::make_unique<FakeTcpTunnelConnection>(
            m_remotePeerId, std::move(*m_address), m_socketsPerConnection);

        m_address = boost::none;
        handler(SystemError::noError, std::move(connection));
    }

    void pleaseStop(std::function<void()> handler) override
    {
        m_ioThreadSocket->pleaseStop(std::move(handler));
    }

    String m_remotePeerId;
    boost::optional<SocketAddress> m_address;
    size_t m_socketsPerConnection;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
};

/** Creates @param nAcceptors @class FakeTcpTunnelAcceptor(s) instead of real ones */
template<quint16 nAcceptors>
struct CloudServerSocketTcpTester: CloudServerSocket
{
    CloudServerSocketTcpTester(): CloudServerSocket(nullptr) {}

    bool listen(int queueLen) override
    {
        initTunnelPool(queueLen);
        for (quint16 i = 0; i < nAcceptors; i++)
        {
            auto addr = nx::network::test::kServerAddress;

            // IP based acceptor
            startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                "somePeerId", SocketAddress(addr.address, addr.port + i)));

            // also start useless acceptors
            startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                "somePeerId", boost::none));
        }

        return true;
    }
};

template<typename ServerSocket>
struct CloudServerSocketTcpTest
    : public ::testing::Test
{
protected:
    std::unique_ptr<ServerSocket> makeServerSocket()
    {
        return std::make_unique<ServerSocket>();
    }
};

struct CloudServerSocketSingleTcpTest
    : CloudServerSocketTcpTest<CloudServerSocketTcpTester<1>> {};

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, CloudServerSocketSingleTcpTest,
    [this](){ return makeServerSocket(); },
    &std::make_unique<TCPSocket>);

struct CloudServerSocketMultiTcpAcceptorTest
    : CloudServerSocketTcpTest<CloudServerSocketTcpTester<5>> {};

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, CloudServerSocketMultiTcpAcceptorTest,
    [this](){ return makeServerSocket(); },
    &std::make_unique<network::test::MultipleClientSocketTester<3>>);

void testTunnelConnect(SystemError::ErrorCode expectedResult)
{
    auto client = std::make_unique<TCPSocket>(false);
    ASSERT_TRUE(client->setNonBlockingMode(true));
    ASSERT_TRUE(client->setSendTimeout(500));

    std::promise<SystemError::ErrorCode> result;
    client->connectAsync(
        network::test::kServerAddress,
        [&](SystemError::ErrorCode c){ result.set_value(c); });

    EXPECT_EQ(result.get_future().get(), expectedResult);
    client->pleaseStopSync();
}

TEST(CloudServerSocketBaseTcpTest, OpenTunnelOnIndication)
{
    auto stunAsyncClient = std::make_shared<network::test::StunAsyncClientMock>();
    EXPECT_CALL(*stunAsyncClient, setIndicationHandler(
        stun::cc::indications::connectionRequested, ::testing::_)).Times(1);

    std::vector<CloudServerSocket::AcceptorMaker> acceptorMakers;
    acceptorMakers.push_back([](
        const String&, hpm::api::ConnectionRequestedEvent& event)
    {
        return std::make_unique<FakeTcpTunnelAcceptor>(
            event.originatingPeerID, network::test::kServerAddress);
    });

    auto server = std::make_unique<CloudServerSocket>(
        std::make_shared<hpm::api::MediatorServerTcpConnection>(
            stunAsyncClient, nullptr),
        std::move(acceptorMakers));
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->listen(1));

    // there is not tunnel yet
    testTunnelConnect(SystemError::connectionRefused);

    stun::Message message(stun::Header(
        stun::MessageClass::indication, stun::cc::indications::connectionRequested));
    hpm::api::ConnectionRequestedEvent event;
    event.connectSessionId = String("someSessionId");
    event.originatingPeerID = String("somePeerId");
    event.serialize(&message);
    stunAsyncClient->emulateIndication(message);

    // now we can use estabilished tunnel
    testTunnelConnect(SystemError::noError);
    server->pleaseStopSync();
}

static size_t kThreadCount = 5;
static size_t kClientCount = 15;
static size_t kPeerCount = 3;

class CloudServerSocketStressTcpTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto addr = network::test::kServerAddress;
        for (size_t i = 0; i < kPeerCount; i++)
            m_peerAddresses.emplace(
                QnUuid::createUuid().toSimpleString().toUtf8(),
                SocketAddress(addr.address, addr.port + i));

        m_stunClient = std::make_shared<network::test::StunAsyncClientMock>();
        EXPECT_CALL(*m_stunClient, setIndicationHandler(
            stun::cc::indications::connectionRequested, ::testing::_)).Times(1);

        std::vector<CloudServerSocket::AcceptorMaker> acceptorMakers;
        acceptorMakers.push_back([this](
            const String&, hpm::api::ConnectionRequestedEvent& event)
        {
            const auto it = m_peerAddresses.find(event.originatingPeerID);
            assert(it != m_peerAddresses.end());

            return std::make_unique<FakeTcpTunnelAcceptor>(
                it->first, it->second, kClientCount);
        });

        m_server = std::make_unique<CloudServerSocket>(
            std::make_shared<hpm::api::MediatorServerTcpConnection>(
                m_stunClient, nullptr),
            std::move(acceptorMakers));
        ASSERT_TRUE(m_server->setNonBlockingMode(true));
        ASSERT_TRUE(m_server->listen(100));
        acceptServerForever();
    }

    void acceptServerForever()
    {
        m_server->acceptAsync([this](
            SystemError::ErrorCode c, AbstractStreamSocket* s)
        {
            {
                QnMutexLocker lock(&m_mutex);
                m_acceptedSockets.push_back(
                    std::unique_ptr<AbstractStreamSocket>(s));
            }

            ASSERT_EQ(c, SystemError::noError);
            acceptServerForever();
        });
    }

    void startClientThread(size_t clientCount)
    {
        std::thread thread([this, clientCount]()
        {
            for (int i = 0; i < clientCount; ++i)
                for (auto it = m_peerAddresses.begin();
                    it != m_peerAddresses.end(); ++it)
                        connectClient(it);
        });

        m_threads.push_back(std::move(thread));
    }

    void connectClient(
        std::map<String, SocketAddress>::iterator peer,
        std::shared_ptr<size_t> counter = std::make_shared<size_t>(0))
    {
        AbstractStreamSocket* socket;
        {
            auto socketPtr = std::make_unique<TCPSocket>();
            socket = socketPtr.get();
            ASSERT_TRUE(socketPtr->setSendTimeout(100));
            ASSERT_TRUE(socketPtr->setRecvTimeout(100));
            ASSERT_TRUE(socketPtr->setNonBlockingMode(true));

            QnMutexLocker lock(&m_mutex);
            m_connectSockets.emplace(socket, std::move(socketPtr));
        }

        socket->connectAsync(peer->second, [this, socket, peer, counter](
            SystemError::ErrorCode c)
        {
            {
                QnMutexLocker lock(&m_mutex);
                m_connectSockets.erase(socket);
            }

            if (c == SystemError::noError)
                return m_connectedResutls.push(*counter);

            ++(*counter);
            requestAcceptor(peer->first);
            connectClient(peer, counter);
        });
    }

    void requestAcceptor(String peerId)
    {
        NX_LOGX(lm("request acceptor from %1").arg(peerId), cl_logDEBUG2);

        hpm::api::ConnectionRequestedEvent event;
        event.connectSessionId = String("someSessionId");
        event.originatingPeerID = std::move(peerId);

        stun::Message message(stun::Header(
            stun::MessageClass::indication,
            stun::cc::indications::connectionRequested));
        event.serialize(&message);

        m_stunClient->emulateIndication(message);
    }

    void TearDown() override
    {
        const auto kTotalConnects = kThreadCount * kClientCount * kPeerCount;
        size_t indicationsCount = 0;
        for (int i = 0; i < kTotalConnects; ++i)
            indicationsCount += m_connectedResutls.pop();

        EXPECT_GE(indicationsCount, kPeerCount);
        m_server->pleaseStopSync();
        for (auto& thread : m_threads)
            thread.join();
    }

    std::map<String, SocketAddress> m_peerAddresses;
    std::shared_ptr<network::test::StunAsyncClientMock> m_stunClient;
    std::unique_ptr<AbstractStreamServerSocket> m_server;
    SyncQueue<size_t> m_connectedResutls;
    std::vector<std::thread> m_threads;

    QnMutex m_mutex;
    std::vector<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
    std::map<void*, std::unique_ptr<AbstractStreamSocket>> m_connectSockets;
};

TEST_F(CloudServerSocketStressTcpTest, SingleThread)
{
    startClientThread(kThreadCount * kClientCount);
}

TEST_F(CloudServerSocketStressTcpTest, MultiThread)
{
    for (int i = 0; i < kThreadCount; ++i)
        startClientThread(kClientCount);
}

} // namespace cloud
} // namespace network
} // namespace nx
