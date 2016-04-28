
#include <gtest/gtest.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <libconnection_mediator/src/test_support/socket_globals_holder.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/test_support/stun_async_client_mock.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>


namespace nx {
namespace network {
namespace cloud {

/**
 * Accepts usual TCP connections
 */
class FakeTcpTunnelConnection
:
    public AbstractIncomingTunnelConnection
{
public:
    FakeTcpTunnelConnection(
        String connectionId, SocketAddress address, size_t sockets = 1000)
    :
        AbstractIncomingTunnelConnection(std::move(connectionId)),
        m_sockets(sockets),
        m_server(std::make_unique<TCPServerSocket>())
    {
        NX_CRITICAL(m_server->setNonBlockingMode(true));
        NX_CRITICAL(m_server->setReuseAddrFlag(true));
        if (!m_server->bind(address) || !m_server->listen())
            m_sockets = 0;

        NX_LOGX(lm("for %1 listen %2 for %3 sockets")
                .arg(connectionId).arg(address.toString())
                .arg(m_sockets), cl_logDEBUG1);
    }

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> handler) override
    {
        if (m_sockets == 0)
            return m_server->post(
                [handler]()
                {
                    handler(SystemError::connectionReset, nullptr);
                });

        m_server->acceptAsync(
            [this, handler](
                SystemError::ErrorCode c, AbstractStreamSocket* s)
            {
                EXPECT_EQ(c, SystemError::noError);
                --m_sockets;
                handler(c, std::unique_ptr<AbstractStreamSocket>(s));
            });
    }

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_server->pleaseStop(
            [this, handler = std::move(handler)]
            {
                NX_LOGX(lm("exhausted"), cl_logDEBUG1);
                m_server.reset();
                handler();
            });
    }

private:
    size_t m_sockets;
    std::unique_ptr<AbstractStreamServerSocket> m_server;
};

/**
 * Creates several \class FakeTcpTunnelConnection
 */
struct FakeTcpTunnelAcceptor
:
    public AbstractTunnelAcceptor
{
    FakeTcpTunnelAcceptor(
        boost::optional<SocketAddress> address,
        size_t socketsPerConnection = 1000)
    :
        m_address(std::move(address)),
        m_socketsPerConnection(socketsPerConnection),
        m_ioThreadSocket(new TCPSocket)
    {
    }

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)> handler) override
    {
        if (!m_address)
            return m_ioThreadSocket->registerTimer(
                100, [handler](){ handler(SystemError::timedOut, nullptr); });

        auto connection = std::make_unique<FakeTcpTunnelConnection>(
            m_remotePeerId, std::move(*m_address), m_socketsPerConnection);

        m_address = boost::none;
        handler(SystemError::noError, std::move(connection));
    }

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_ioThreadSocket->pleaseStop(std::move(handler));
    }

    boost::optional<SocketAddress> m_address;
    size_t m_socketsPerConnection;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
};

/**
 * Creates @param nAcceptors @class FakeTcpTunnelAcceptor(s) instead of real ones
 */
template<quint16 nAcceptors>
struct CloudServerSocketTcpTester
:
    CloudServerSocket
{
    CloudServerSocketTcpTester()
    :
        CloudServerSocket(
            nx::network::SocketGlobals::mediatorConnector().systemConnection())
    {
    }

    bool listen(int queueLen) override
    {
        initTunnelPool(queueLen);
        moveToListeningState();
        for (quint16 i = 0; i < nAcceptors; i++)
        {
            auto addr = nx::network::test::kServerAddress;

            // IP based acceptor
            startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                SocketAddress(addr.address, addr.port + i)));

            // also start useless acceptors
            startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                boost::none));
        }

        return true;
    }
};

class CloudServerSocketTest
:
    public ::testing::Test
{
public:
    CloudServerSocketTest()
    {
        SocketGlobalsHolder::instance()->reinitialize();
        init();
    }

private:
    hpm::MediatorFunctionalTest m_mediator;

    void init()
    {
        ASSERT_TRUE(m_mediator.startAndWaitUntilStarted());
        auto system = m_mediator.addRandomSystem();
        auto server = m_mediator.addRandomServerNotRegisteredOnMediator(system);
        ASSERT_NE(nullptr, server);

        SocketGlobals::mediatorConnector().setSystemCredentials(
            nx::hpm::api::SystemCredentials(
                system.id,
                server->serverId(),
                system.authKey));
        SocketGlobals::mediatorConnector().mockupAddress(m_mediator.endpoint());
        SocketGlobals::mediatorConnector().enable(true);
    }
};

template<typename ServerSocket>
class CloudServerSocketTcpTest
:
    public CloudServerSocketTest
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

static void testTunnelConnect(bool isSuccessExpected)
{
    auto client = std::make_unique<TCPSocket>(false);
    ASSERT_TRUE(client->setNonBlockingMode(true));
    ASSERT_TRUE(client->setSendTimeout(500));

    nx::utils::promise<SystemError::ErrorCode> result;
    client->connectAsync(
        network::test::kServerAddress,
        [&](SystemError::ErrorCode c){ result.set_value(c); });

    const auto code = result.get_future().get();
    EXPECT_EQ(code == SystemError::noError, isSuccessExpected) << code;
    client->pleaseStopSync();
}

class CloudServerSocketBaseTcpTest
:
    public CloudServerSocketTest
{
};

TEST_F(CloudServerSocketBaseTcpTest, OpenTunnelOnIndication)
{
    auto stunAsyncClient = std::make_shared<network::test::StunAsyncClientMock>();
    EXPECT_CALL(*stunAsyncClient, setIndicationHandler(
        stun::cc::indications::connectionRequested, ::testing::_)).Times(1);
    EXPECT_CALL(*stunAsyncClient, remoteAddress()).Times(::testing::AnyNumber());

    std::vector<CloudServerSocket::AcceptorMaker> acceptorMakers;
    acceptorMakers.push_back(
        [](hpm::api::ConnectionRequestedEvent)
        {
            return std::make_unique<FakeTcpTunnelAcceptor>(
                network::test::kServerAddress);
        });

    auto server = std::make_unique<CloudServerSocket>(
        std::make_shared<hpm::api::MediatorServerTcpConnection>(
            stunAsyncClient,
            &nx::network::SocketGlobals::mediatorConnector()),
        nx::network::RetryPolicy(),
        std::move(acceptorMakers));
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->listen(1));
    server->moveToListeningState();

    // there is no tunnel yet
    testTunnelConnect(false);

    hpm::api::ConnectionRequestedEvent event;
    event.connectSessionId = String("someSessionId");
    event.originatingPeerID = String("somePeerId");

    stun::Message message(stun::Header(
        stun::MessageClass::indication,
        stun::cc::indications::connectionRequested));
    event.serialize(&message);
    stunAsyncClient->emulateIndication(message);

    //giving server socket time to process indication
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // now we can use estabilished tunnel
    testTunnelConnect(true);
    server->pleaseStopSync();
}

static const size_t kThreadCount = 5;
static const size_t kClientCount = 15;
static const size_t kPeerCount = 3;
static const size_t kQueueLimit = 10;
static const size_t kRetryDelay = 100;

class CloudServerSocketStressTcpTest
:
    public ::testing::Test
{
protected:
    void SetUp() override
    {
        //there must be cloud credentials if we use CloudServerSocket
        nx::hpm::api::SystemCredentials systemCredentials;
        systemCredentials.systemId = "system";
        systemCredentials.key = "key";
        SocketGlobals::mediatorConnector().setSystemCredentials(
            std::move(systemCredentials));

        const auto addr = network::test::kServerAddress;
        for (size_t i = 0; i < kPeerCount; i++)
            m_peerAddresses.emplace(
                QnUuid::createUuid().toSimpleString().toUtf8(),
                SocketAddress(addr.address, addr.port + i));

        m_stunClient = std::make_shared<network::test::StunAsyncClientMock>();
        EXPECT_CALL(*m_stunClient, setIndicationHandler(
            stun::cc::indications::connectionRequested, ::testing::_)).Times(1);
        EXPECT_CALL(*m_stunClient, remoteAddress()).Times(::testing::AnyNumber());

        std::vector<CloudServerSocket::AcceptorMaker> acceptorMakers;
        acceptorMakers.push_back(
            [this](hpm::api::ConnectionRequestedEvent event)
            {
                const auto it = m_peerAddresses.find(event.originatingPeerID);
                NX_ASSERT(it != m_peerAddresses.end());

                return std::make_unique<FakeTcpTunnelAcceptor>(
                    it->second, kClientCount);
            });

        auto cloudServerSocket = std::make_unique<CloudServerSocket>(
            std::make_shared<hpm::api::MediatorServerTcpConnection>(
                m_stunClient, &SocketGlobals::mediatorConnector()),
            nx::network::RetryPolicy(),
            std::move(acceptorMakers));
        ASSERT_TRUE(cloudServerSocket->setNonBlockingMode(true));
        ASSERT_TRUE(cloudServerSocket->listen(10));
        cloudServerSocket->moveToListeningState();
        m_server = std::move(cloudServerSocket);
        acceptServerForever();
    }

    void acceptServerForever()
    {
        m_server->acceptAsync(
            [this](SystemError::ErrorCode code, AbstractStreamSocket* socket)
            {
                ASSERT_EQ(code, SystemError::noError);
                acceptServerForever();
                socket->sendAsync(
                    network::test::kTestMessage,
                    [](SystemError::ErrorCode code, size_t size)
                    {
                        ASSERT_EQ(code, SystemError::noError);
                        ASSERT_EQ(size, network::test::kTestMessage.size());
                    });

                {
                    QnMutexLocker lock(&m_mutex);
                    m_acceptedSockets.push_back(
                        std::unique_ptr<AbstractStreamSocket>(socket));
                }
            });
    }

    void startClientThread(size_t clientCount)
    {
        nx::utils::thread thread(
            [this, clientCount]()
            {
                for (size_t i = 0; i < clientCount; ++i)
                    for (auto it = m_peerAddresses.begin();
                        it != m_peerAddresses.end(); ++it)
                            connectClient(it);
            });

        m_threads.push_back(std::move(thread));
    }

    struct Counters
    {
        size_t indications, retries;

        Counters(size_t i = 0, size_t r = 0): indications(i), retries(r) {}
        Counters addIndication() const { return {indications + 1, retries }; }
        Counters addRetry() const { return {indications, retries + 1 }; }
    };

    void connectClient(
        std::map<String, SocketAddress>::iterator peer,
        Counters counter = Counters())
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

        socket->connectAsync(
            peer->second,
            [this, socket, peer, counter](SystemError::ErrorCode code)
            {
                if (code != SystemError::noError)
                    return retryOnClient(
                        socket,
                        [this, peer, counter]()
                        {
                            requestAcceptor(peer->first);
                            connectClient(peer, counter.addIndication());
                        });

                readOnClient(socket, peer, counter);
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

    void readOnClient(
        AbstractStreamSocket* socket,
        std::map<String, SocketAddress>::iterator peer,
        Counters counter)
    {
        auto buffer = std::make_shared<Buffer>();
        buffer->reserve(network::test::kTestMessage.size() + 1);
        socket->readSomeAsync(
            buffer.get(),
            [this, socket, peer, counter, buffer](
                SystemError::ErrorCode code, size_t size)
            {
                if (code != SystemError::noError ||
                    size != static_cast<size_t>(
                        network::test::kTestMessage.size()))
                {
                    return retryOnClient(
                        socket,
                        [this, peer, counter]()
                        {
                            connectClient(peer, counter.addRetry());
                        });
                }

                {
                    QnMutexLocker lock(&m_mutex);
                    m_connectSockets.erase(socket);
                }

                EXPECT_EQ(*buffer, network::test::kTestMessage);
                m_connectedResults.push(counter);
            });
    }

    void retryOnClient(AbstractStreamSocket* socket, std::function<void()> action)
    {
        socket->registerTimer(
            300, [this, socket, action = std::move(action)]()
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    m_connectSockets.erase(socket);
                }

                action();
            });
    }

    void TearDown() override
    {
        const auto kTotalConnects = kThreadCount * kClientCount * kPeerCount;
        Counters totalCounters;
        for (size_t i = 0; i < kTotalConnects; ++i)
        {
            const auto c = m_connectedResults.pop();
            totalCounters.indications += c.indications;
            totalCounters.retries += c.retries;
        }

        EXPECT_GE(totalCounters.indications, kPeerCount * kThreadCount);
        EXPECT_GE(totalCounters.retries, kTotalConnects / kQueueLimit);

        m_server->pleaseStopSync();
        for (auto& thread : m_threads)
            thread.join();

        EXPECT_GE(m_acceptedSockets.size(), kTotalConnects);
        for (auto& socket : m_acceptedSockets)
            socket->pleaseStopSync();
    }

    std::map<String, SocketAddress> m_peerAddresses;
    std::shared_ptr<network::test::StunAsyncClientMock> m_stunClient;
    std::unique_ptr<AbstractStreamServerSocket> m_server;
    TestSyncQueue<Counters> m_connectedResults;
    std::vector<nx::utils::thread> m_threads;

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
    for (size_t i = 0; i < kThreadCount; ++i)
        startClientThread(kClientCount);
}

//TEST_F(CloudServerSocketTest, aioCancellation)
//{
//    CloudServerSocket sock(
//        nx::network::SocketGlobals::mediatorConnector().systemConnection());
//    sock.listen(128);
//    
//    sock.acceptAsync();
//}

} // namespace cloud
} // namespace network
} // namespace nx
