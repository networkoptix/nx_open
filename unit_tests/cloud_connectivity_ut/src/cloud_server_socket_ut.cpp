
#include <gtest/gtest.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
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
        aio::AbstractAioThread* thread,
        network::test::AddressBinder::Manager addressManager,
        size_t clientsLimit)
    :
        AbstractIncomingTunnelConnection(
            addressManager.key.address.toString().toUtf8()),
        m_clientsLimit(clientsLimit),
        m_server(new TCPServerSocket),
        m_addressManager(std::move(addressManager))
    {
        m_server->bindToAioThread(thread);
        NX_CRITICAL(m_server->setNonBlockingMode(true));
        NX_CRITICAL(m_server->setReuseAddrFlag(true));
        NX_CRITICAL(m_server->bind(network::test::kAnyPrivateAddress));
        NX_CRITICAL(m_server->listen());

        auto address = m_server->getLocalAddress();
        NX_LOGX(lm("listening %1 for %2 sockets")
            .str(address.toString()).arg(m_clientsLimit), cl_logDEBUG1);
        m_addressManager.add(std::move(address));
    }

    ~FakeTcpTunnelConnection()
    {
        m_addressManager.remove(m_server->getLocalAddress());
        NX_LOGX(lm("removed, %1 sockets left").arg(m_clientsLimit), cl_logDEBUG1);
    }

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> handler) override
    {
        if (m_clientsLimit == 0)
        {
            return m_server->dispatch(
                [handler]() { handler(SystemError::connectionReset, nullptr); });
        }

        m_server->acceptAsync(
            [this, handler](
                SystemError::ErrorCode c, AbstractStreamSocket* s)
            {
                EXPECT_EQ(c, SystemError::noError);
                --m_clientsLimit;
                NX_LOGX(lm("accepted, %1 left").arg(m_clientsLimit), cl_logDEBUG2);
                handler(c, std::unique_ptr<AbstractStreamSocket>(s));
            });
    }

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_server->pleaseStop(std::move(handler));
    }

private:
    size_t m_clientsLimit;
    std::unique_ptr<AbstractStreamServerSocket> m_server;
    network::test::AddressBinder::Manager m_addressManager;
};

/**
 * Creates several \class FakeTcpTunnelConnection
 */
struct FakeTcpTunnelAcceptor
:
    public AbstractTunnelAcceptor
{
    FakeTcpTunnelAcceptor(
        network::test::AddressBinder::Manager addressManager,
        aio::AbstractAioThread* designatedAioThread = nullptr,
        bool hasConnection = true,
        size_t clientsLimit = 5)
    :
        m_designatedAioThread(designatedAioThread),
        m_ioThreadSocket(new TCPSocket),
        m_addressManager(std::move(addressManager)),
        m_hasConnection(hasConnection),
        m_clientsLimit(clientsLimit)
    {
        if (designatedAioThread)
            m_ioThreadSocket->bindToAioThread(designatedAioThread);

        NX_LOGX(lm("prepare to listen '%1', c=%2, lim=%3, thread=%4")
            .str(addressManager.key).arg(hasConnection).arg(clientsLimit)
            .arg(designatedAioThread), cl_logDEBUG1);
    }

    ~FakeTcpTunnelAcceptor()
    {
        NX_LOGX(lm("removed, c=%1").str(m_hasConnection), cl_logDEBUG1);
    }

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)> handler) override
    {
        if (!m_designatedAioThread)
        {
            m_designatedAioThread = m_mediatorConnection->getAioThread();
            m_ioThreadSocket->bindToAioThread(m_designatedAioThread);
        }

        m_ioThreadSocket->dispatch([this, handler = std::move(handler)]()
        {
            if (!m_hasConnection)
                return m_ioThreadSocket->registerTimer(
                    100, [handler](){ handler(SystemError::timedOut, nullptr); });

            auto connection = std::make_unique<FakeTcpTunnelConnection>(
                m_ioThreadSocket->getAioThread(), m_addressManager, m_clientsLimit);

            m_hasConnection = false;
            handler(SystemError::noError, std::move(connection));
        });
    }

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_ioThreadSocket->pleaseStop(std::move(handler));
    }

    aio::AbstractAioThread* m_designatedAioThread;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
    network::test::AddressBinder::Manager m_addressManager;
    bool m_hasConnection;
    size_t m_clientsLimit;
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

protected:
    hpm::MediatorFunctionalTest m_mediator;

private:
    void init()
    {
        ASSERT_TRUE(m_mediator.startAndWaitUntilStarted());
        auto system = m_mediator.addRandomSystem();
        auto server = m_mediator.addRandomServerNotRegisteredOnMediator(system);
        ASSERT_NE(nullptr, server);

        stun::AbstractAsyncClient::Settings stunClientSettings;
        stunClientSettings.reconnectPolicy =
            nx::network::RetryPolicy(
                nx::network::RetryPolicy::kInfiniteRetries,
                std::chrono::milliseconds(0),
                nx::network::RetryPolicy::kDefaultDelayMultiplier,
                std::chrono::minutes(1));
        SocketGlobals::mediatorConnector().reinitializeStunClient(stunClientSettings);
        SocketGlobals::mediatorConnector().setSystemCredentials(
            nx::hpm::api::SystemCredentials(
                system.id,
                server->serverId(),
                system.authKey));
        SocketGlobals::mediatorConnector().mockupAddress(m_mediator.stunEndpoint());
        SocketGlobals::mediatorConnector().enable(true);
    }
};

struct CloudServerSocketTcpTester
:
    public CloudServerSocket
{
    CloudServerSocketTcpTester(network::test::AddressBinder* addressBinder)
    :
        CloudServerSocket(
            nx::network::SocketGlobals::mediatorConnector().systemConnection()),
        m_addressManager(addressBinder)
    {
    }

    SocketAddress getLocalAddress() const override
    {
        return m_addressManager.key;
    }

    bool listen(int queueLen) override
    {
        initTunnelPool(queueLen);
        moveToListeningState();
        m_mediatorRegistrationRetryTimer.dispatch(
            [this]()
            {
                for (size_t i = 0; i < 2; ++i)
                {
                    // normal acceptor
                    startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                        m_addressManager, getAioThread(), true, 100));

                    // acceptor without sockets
                    startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                        m_addressManager, getAioThread(), true, 0));

                    // brocken acceptor
                    startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                        m_addressManager, getAioThread(), false));
                }
            });

        return true;
    }

    network::test::AddressBinder::Manager m_addressManager;
};

class CloudServerSocketTcpTest
:
    public ::testing::Test
{
protected:
    std::unique_ptr<CloudServerSocketTcpTester> makeServerTester()
    {
        return std::make_unique<CloudServerSocketTcpTester>(
            &addressBinder);
    }

    std::unique_ptr<network::test::MultipleClientSocketTester> makeClientTester()
    {
        return std::make_unique<network::test::MultipleClientSocketTester>(
            &addressBinder);
    }

    network::test::AddressBinder addressBinder;
};

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, CloudServerSocketTcpTest,
    [this](){ return makeServerTester(); },
    [this](){ return makeClientTester(); });

TEST_F(CloudServerSocketTcpTest, OpenTunnelOnIndication)
{
    network::test::AddressBinder addressBinder;
    const network::test::AddressBinder::Manager addressManager(&addressBinder);

    auto stunAsyncClient = std::make_shared<stun::test::AsyncClientMock>();
    EXPECT_CALL(*stunAsyncClient, setIndicationHandler(
        stun::cc::indications::connectionRequested,
        ::testing::_, ::testing::_)).Times(1);
    EXPECT_CALL(*stunAsyncClient, remoteAddress()).Times(::testing::AnyNumber());

    std::vector<CloudServerSocket::AcceptorMaker> acceptorMakers;
    acceptorMakers.push_back(
        [&](hpm::api::ConnectionRequestedEvent)
        {
            return std::make_unique<FakeTcpTunnelAcceptor>(addressManager);
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

    // there is no tunnels yet
    ASSERT_EQ(addressBinder.get(addressManager.key).size(), 0);

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

    auto list = addressBinder.get(addressManager.key);
    ASSERT_EQ(list.size(), 1);

    auto client = std::make_unique<TCPSocket>(false);
    ASSERT_TRUE(client->setNonBlockingMode(true));
    ASSERT_TRUE(client->setSendTimeout(500));

    nx::utils::promise<SystemError::ErrorCode> result;
    client->connectAsync(
        *list.begin(),
        [&](SystemError::ErrorCode c){ result.set_value(c); });

    EXPECT_EQ(result.get_future().get(), SystemError::noError);
    client->pleaseStopSync();
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

        const auto addr = network::test::kAnyPrivateAddress;
        for (size_t i = 0; i < kPeerCount; i++)
            m_boundPeers.insert(m_addressBinder.bind());

        m_stunClient = std::make_shared<stun::test::AsyncClientMock>();
        EXPECT_CALL(*m_stunClient, setIndicationHandler(
            stun::cc::indications::connectionRequested,
            ::testing::_, ::testing::_)).Times(1);
        EXPECT_CALL(*m_stunClient, remoteAddress()).Times(::testing::AnyNumber());

        std::vector<CloudServerSocket::AcceptorMaker> acceptorMakers;
        acceptorMakers.push_back(
            [this](hpm::api::ConnectionRequestedEvent event)
            {
                SocketAddress address(QLatin1String(event.originatingPeerID));
                return std::make_unique<FakeTcpTunnelAcceptor>(
                    network::test::AddressBinder::Manager(
                        &m_addressBinder, std::move(address)));
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
            [=]()
            {
                for (size_t i = 0; i < clientCount; ++i)
                {
                    for (const auto peer: m_boundPeers)
                        startClient(peer);
                }
            });

        m_threads.push_back(std::move(thread));
    }

    void startClient(const SocketAddress& peer)
    {
        auto socketPtr = std::make_unique<TCPSocket>();
        auto socket = socketPtr.get();
        ASSERT_TRUE(socketPtr->setSendTimeout(100));
        ASSERT_TRUE(socketPtr->setRecvTimeout(100));
        ASSERT_TRUE(socketPtr->setNonBlockingMode(true));

        {
            QnMutexLocker lock(&m_mutex);
            m_connectSockets.emplace(socket, std::move(socketPtr));
        }

        NX_LOGX(lm("start %1 -> %2").arg(socket).str(peer), cl_logDEBUG1);
        connectClient(socket, peer);
    }

    void connectClient(AbstractStreamSocket* socket, const SocketAddress& peer)
    {
        if (auto address = m_addressBinder.random(peer))
        {
            NX_LOGX(lm("connect %1 -> %2").arg(socket).str(*address), cl_logDEBUG2);
            socket->connectAsync(
                address.get(),
                [=](SystemError::ErrorCode code)
                {
                    if (code == SystemError::noError)
                        return readOnClient(socket, peer);

                    socket->registerTimer(
                        200, [=](){ connectClient(socket, peer); });
                });
        }
        else
        {
            socket->registerTimer(
                rand() % 1000 + 100,
                [=]()
                {
                    if (auto address = m_addressBinder.random(peer))
                        return connectClient(socket, peer);

                    NX_LOGX(lm("indicate %1 -> %2").arg(socket).str(peer), cl_logDEBUG2);
                    emitIndication(peer);
                    socket->registerTimer(
                        200, [=](){ connectClient(socket, peer); });
                });
        }
    }

    void emitIndication(SocketAddress peer)
    {
        String peerId = peer.address.toString().toUtf8();

        hpm::api::ConnectionRequestedEvent event;
        event.connectSessionId = String("someSessionId");
        event.originatingPeerID = std::move(peerId);

        stun::Message message(stun::Header(
            stun::MessageClass::indication,
            stun::cc::indications::connectionRequested));
        event.serialize(&message);

        m_stunClient->emulateIndication(message);
    }

    void readOnClient(AbstractStreamSocket* socket, const SocketAddress& peer)
    {
        auto buffer = std::make_shared<Buffer>();
        buffer->reserve(network::test::kTestMessage.size() + 1);
        socket->readSomeAsync(
            buffer.get(),
            [=](SystemError::ErrorCode code, size_t size)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    m_connectSockets.erase(socket);
                }

                if (code != SystemError::noError ||
                    size != static_cast<size_t>(
                        network::test::kTestMessage.size()))
                {
                    NX_LOGX(lm("read %1 failed").arg(socket), cl_logDEBUG2);
                    return startClient(peer);
                }

                NX_LOGX(lm("read %1 successed").arg(socket), cl_logDEBUG2);
                EXPECT_EQ(*buffer, network::test::kTestMessage);
                m_connectedResults.push(code);
            });
    }

    void TearDown() override
    {
        const auto kTotalConnects = kThreadCount * kClientCount * kPeerCount;
        for (size_t i = 0; i < kTotalConnects; ++i)
            m_connectedResults.pop();

        m_server->pleaseStopSync();
        for (auto& thread : m_threads)
            thread.join();

        EXPECT_GE(m_acceptedSockets.size(), kTotalConnects);
        for (auto& socket : m_acceptedSockets)
            socket->pleaseStopSync();
    }

    network::test::AddressBinder m_addressBinder;
    std::set<SocketAddress> m_boundPeers;

    std::shared_ptr<stun::test::AsyncClientMock> m_stunClient;
    std::unique_ptr<AbstractStreamServerSocket> m_server;
    TestSyncQueue<SystemError::ErrorCode> m_connectedResults;
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

TEST_F(CloudServerSocketTest, reconnect)
{
    hpm::api::SystemCredentials currentCredentials =
        *nx::network::SocketGlobals::mediatorConnector().getSystemCredentials();

    auto system = m_mediator.addRandomSystem();
    auto server = m_mediator.addRandomServerNotRegisteredOnMediator(system);

    hpm::api::SystemCredentials otherCredentials;
    otherCredentials.systemId = system.id;
    otherCredentials.key = system.authKey;
    otherCredentials.serverId = server->serverId();

    for (int i = 0; i < 200; ++i)
    {
        CloudServerSocket cloudServerSocket(
            nx::network::SocketGlobals::mediatorConnector().systemConnection());
        ASSERT_EQ(cloudServerSocket.registerOnMediatorSync(), hpm::api::ResultCode::ok);
        ASSERT_TRUE(cloudServerSocket.listen(128));
        cloudServerSocket.moveToListeningState();

        //breaking connection to mediator
        nx::network::SocketGlobals::mediatorConnector().setSystemCredentials(otherCredentials);
        std::swap(currentCredentials, otherCredentials);

        cloudServerSocket.pleaseStopSync();
    }
}

} // namespace cloud
} // namespace network
} // namespace nx
