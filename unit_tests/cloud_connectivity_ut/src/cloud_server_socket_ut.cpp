#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/tunnel_acceptor_factory.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/acceptor_stub.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/test_support/stun_async_client_mock.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/test_options.h>

#include <libconnection_mediator/src/listening_peer_pool.h>
#include <libconnection_mediator/src/mediator_service.h>
#include <libconnection_mediator/src/test_support/mediator_functional_test.h>

#include "predefined_mediator_connector.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

/**
 * Accepts usual TCP connections
 */
class FakeTcpTunnelConnection:
    public AbstractIncomingTunnelConnection
{
    using base_type = AbstractIncomingTunnelConnection;

public:
    FakeTcpTunnelConnection(
        aio::AbstractAioThread* aioThread,
        network::test::AddressBinder::Manager addressManager,
        size_t clientsLimit)
    :
        m_clientsLimit(clientsLimit),
        m_server(new TCPServerSocket(AF_INET)),
        m_addressManager(std::move(addressManager))
    {
        bindToAioThread(getAioThread());
        init(aioThread);
    }

    ~FakeTcpTunnelConnection()
    {
        stopWhileInAioThread();
        NX_LOGX(lm("removed, %1 sockets left").arg(m_clientsLimit), cl_logDEBUG1);
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_server->bindToAioThread(aioThread);
    }

    void accept(AcceptHandler handler) override
    {
        if (m_clientsLimit == 0)
        {
            return m_server->dispatch(
                [handler]() { handler(SystemError::connectionReset, nullptr); });
        }

        m_server->acceptAsync(
            [this, handler](
                SystemError::ErrorCode c, std::unique_ptr<AbstractStreamSocket> s)
            {
                EXPECT_EQ(c, SystemError::noError);
                --m_clientsLimit;
                NX_LOGX(lm("accepted, %1 left").arg(m_clientsLimit), cl_logDEBUG2);
                handler(c, std::move(s));
            });
    }

private:
    size_t m_clientsLimit;
    std::unique_ptr<AbstractStreamServerSocket> m_server;
    network::test::AddressBinder::Manager m_addressManager;

    virtual void stopWhileInAioThread() override
    {
        if (m_server)
            m_addressManager.remove(m_server->getLocalAddress());
        m_server.reset();
    }

    void init(aio::AbstractAioThread* aioThread)
    {
        m_server->bindToAioThread(aioThread);
        ASSERT_TRUE(m_server->setNonBlockingMode(true))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(m_server->bind(SocketAddress::anyPrivateAddress))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(m_server->listen())
            << SystemError::getLastOSErrorText().toStdString();

        auto address = m_server->getLocalAddress();
        NX_LOGX(lm("listening %1 for %2 sockets")
            .arg(address.toString()).arg(m_clientsLimit), cl_logDEBUG1);
        m_addressManager.add(std::move(address));
    }
};

/**
 * Creates several FakeTcpTunnelConnection.
 */
struct FakeTcpTunnelAcceptor:
    public AbstractTunnelAcceptor
{
    FakeTcpTunnelAcceptor(
        network::test::AddressBinder::Manager addressManager,
        aio::AbstractAioThread* designatedAioThread = nullptr,
        bool hasConnection = true,
        size_t clientsLimit = 5)
    :
        m_designatedAioThread(designatedAioThread),
        m_ioThreadSocket(new TCPSocket(AF_INET)),
        m_addressManager(std::move(addressManager)),
        m_hasConnection(hasConnection),
        m_clientsLimit(clientsLimit)
    {
        if (designatedAioThread)
            m_ioThreadSocket->bindToAioThread(designatedAioThread);

        NX_LOGX(lm("prepare to listen '%1', c=%2, lim=%3, thread=%4")
            .arg(addressManager.key).arg(hasConnection).arg(clientsLimit)
            .arg(designatedAioThread), cl_logDEBUG1);
    }

    ~FakeTcpTunnelAcceptor()
    {
        NX_LOGX(lm("removed, c=%1").arg(m_hasConnection), cl_logDEBUG1);
    }

    void accept(AcceptHandler handler) override
    {
        if (!m_designatedAioThread)
        {
            m_designatedAioThread = m_mediatorConnection->getAioThread();
            m_ioThreadSocket->bindToAioThread(m_designatedAioThread);
        }

        m_ioThreadSocket->dispatch(
            [this, handler = std::move(handler)]()
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

    virtual std::string toString() const override
    {
        return "FakeTcpTunnelAcceptor";
    }

    aio::AbstractAioThread* m_designatedAioThread;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
    network::test::AddressBinder::Manager m_addressManager;
    bool m_hasConnection;
    size_t m_clientsLimit;
};

class CloudServerSocketTcpTester:
    public CloudServerSocket
{
public:
    CloudServerSocketTcpTester(network::test::AddressBinder* addressBinder):
        CloudServerSocket(&nx::network::SocketGlobals::cloud().mediatorConnector()),
        m_addressManager(addressBinder)
    {
    }

    virtual SocketAddress getLocalAddress() const override
    {
        return m_addressManager.key;
    }

    virtual bool listen(int queueLen) override
    {
        initTunnelPool(queueLen);
        moveToListeningState();

        utils::promise<void> promise;
        m_mediatorRegistrationRetryTimer.dispatch(
            [this, &promise]()
            {
                for (size_t i = 0; i < 2; ++i)
                {
                    // normal acceptor
                    startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                        m_addressManager, getAioThread(), true, 100));

                    // acceptor without sockets
                    startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                        m_addressManager, getAioThread(), true, 0));

                    // broken acceptor
                    startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                        m_addressManager, getAioThread(), false));
                }

                promise.set_value();
            });

        promise.get_future().wait();
        return true;
    }

private:
    network::test::AddressBinder::Manager m_addressManager;
};

//-------------------------------------------------------------------------------------------------

class CloudServerSocketTcpTest:
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

TEST_F(CloudServerSocketTcpTest, TransferSyncSsl)
{
    network::test::socketTransferSync(
        [&]() { return std::make_unique<deprecated::SslServerSocket>(makeServerTester(), false); },
        [&]() { return std::make_unique<deprecated::SslSocket>(makeClientTester(), false); });
}

TEST_F(CloudServerSocketTcpTest, OpenTunnelOnIndication)
{
    network::test::AddressBinder addressBinder;
    const network::test::AddressBinder::Manager addressManager(&addressBinder);

    auto stunAsyncClient = std::make_shared<stun::test::AsyncClientMock>();
    EXPECT_CALL(*stunAsyncClient, setIndicationHandler(
        stun::extension::indications::connectionRequested,
        ::testing::_, ::testing::_)).Times(1);
    EXPECT_CALL(*stunAsyncClient, remoteAddress()).Times(::testing::AnyNumber());

    auto tunnelAcceptorFactoryFuncBak =
        TunnelAcceptorFactory::instance().setCustomFunc(
            [&addressManager](
                const SocketAddress& /*mediatorUdpEndpoint*/,
                hpm::api::ConnectionRequestedEvent)
            {
                std::vector<std::unique_ptr<AbstractTunnelAcceptor>> acceptors;
                acceptors.push_back(std::make_unique<FakeTcpTunnelAcceptor>(addressManager));
                return acceptors;
            });
    auto tunnelAcceptorFactoryGuard = makeScopeGuard(
        [tunnelAcceptorFactoryFuncBak = std::move(tunnelAcceptorFactoryFuncBak)]() mutable
        {
            TunnelAcceptorFactory::instance().setCustomFunc(
                std::move(tunnelAcceptorFactoryFuncBak));
        });

    PredefinedMediatorConnector mediatorConnector(
        stunAsyncClient->remoteAddress(),
        std::make_unique<hpm::api::MediatorServerTcpConnection>(
            stunAsyncClient,
            &nx::network::SocketGlobals::cloud().mediatorConnector()));

    auto server = std::make_unique<CloudServerSocket>(
        &mediatorConnector,
        nx::network::RetryPolicy());
    auto serverGuard = makeScopeGuard([&server]() { server->pleaseStopSync(); });
    ASSERT_TRUE(server->setNonBlockingMode(true));
    ASSERT_TRUE(server->listen(1));
    server->moveToListeningState();

    // there is no tunnels yet
    ASSERT_EQ(0U, addressBinder.get(addressManager.key).size());

    hpm::api::ConnectionRequestedEvent event;
    event.connectSessionId = String("someSessionId");
    event.originatingPeerID = String("somePeerId");

    stun::Message message(stun::Header(
        stun::MessageClass::indication,
        stun::extension::indications::connectionRequested));
    event.serialize(&message);
    stunAsyncClient->emulateIndication(message);

    //giving server socket time to process indication
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto list = addressBinder.get(addressManager.key);
    ASSERT_EQ(1U, list.size());

    auto client = std::make_unique<TCPSocket>(AF_INET);
    auto clientGuard = makeScopeGuard([&client]() { client->pleaseStopSync(); });
    ASSERT_TRUE(client->setNonBlockingMode(true));

    nx::utils::promise<SystemError::ErrorCode> result;
    client->connectAsync(
        *list.begin(),
        [&](SystemError::ErrorCode c){ result.set_value(c); });

    ASSERT_EQ(SystemError::noError, result.get_future().get());
}

//-------------------------------------------------------------------------------------------------

static const size_t kThreadCount = 5;
static const size_t kClientCount = 15;
static const size_t kPeerCount = 3;
static const size_t kQueueLimit = 10;
static const size_t kRetryDelay = 100;

class CloudServerSocketStressTcpTest:
    public ::testing::Test
{
protected:
    void SetUp() override
    {
        //there must be cloud credentials if we use CloudServerSocket
        nx::hpm::api::SystemCredentials systemCredentials;
        systemCredentials.systemId = "system";
        systemCredentials.key = "key";
        SocketGlobals::cloud().mediatorConnector().setSystemCredentials(
            std::move(systemCredentials));

        for (size_t i = 0; i < kPeerCount; i++)
            m_boundPeers.insert(m_addressBinder.bind());

        m_stunClient = std::make_shared<stun::test::AsyncClientMock>();
        EXPECT_CALL(*m_stunClient, setIndicationHandler(
            stun::extension::indications::connectionRequested,
            ::testing::_, ::testing::_)).Times(1);
        EXPECT_CALL(*m_stunClient, remoteAddress()).Times(::testing::AnyNumber());

        m_tunnelAcceptorFactoryFuncBak =
            TunnelAcceptorFactory::instance().setCustomFunc(
                [this](
                    const SocketAddress& /*mediatorUdpEndpoint*/,
                    hpm::api::ConnectionRequestedEvent event)
                {
                    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> acceptors;

                    SocketAddress address(QLatin1String(event.originatingPeerID));
                    auto acceptor = std::make_unique<FakeTcpTunnelAcceptor>(
                        network::test::AddressBinder::Manager(
                            &m_addressBinder, std::move(address)));
                    acceptors.push_back(std::move(acceptor));
                    return acceptors;
                });

        m_mediatorConnector = std::make_unique<PredefinedMediatorConnector>(
            m_stunClient->remoteAddress(),
            std::make_unique<hpm::api::MediatorServerTcpConnection>(
                m_stunClient,
                &SocketGlobals::cloud().mediatorConnector()));

        auto cloudServerSocket = std::make_unique<CloudServerSocket>(
            m_mediatorConnector.get(),
            nx::network::RetryPolicy());
        ASSERT_TRUE(cloudServerSocket->setNonBlockingMode(true));
        ASSERT_TRUE(cloudServerSocket->listen(10));
        cloudServerSocket->moveToListeningState();
        m_server = std::move(cloudServerSocket);
        acceptServerForever();
    }

    void acceptServerForever()
    {
        m_server->acceptAsync(
            [this](SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
            {
                ASSERT_EQ(code, SystemError::noError);
                acceptServerForever();
                ASSERT_TRUE(socket->setNonBlockingMode(true));
                socket->sendAsync(
                    network::test::kTestMessage,
                    [this, socketPtr = socket.get()](SystemError::ErrorCode code, size_t size)
                    {
                        NX_LOGX(lm("test message is sent to %1").arg(socketPtr),
                            cl_logDEBUG2);

                        ASSERT_EQ(code, SystemError::noError);
                        ASSERT_EQ(size, (size_t)network::test::kTestMessage.size());
                    });

                {
                    QnMutexLocker lock(&m_mutex);
                    m_acceptedSockets.push_back(std::move(socket));
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
        auto socketPtr = std::make_unique<TCPSocket>(AF_INET);
        auto socket = socketPtr.get();
        const auto timeout = 3000 * utils::TestOptions::timeoutMultiplier();
        ASSERT_TRUE(socketPtr->setSendTimeout(timeout));
        ASSERT_TRUE(socketPtr->setRecvTimeout(timeout));
        ASSERT_TRUE(socketPtr->setNonBlockingMode(true));

        {
            QnMutexLocker lock(&m_mutex);
            m_connectSockets.emplace(socket, std::move(socketPtr));
        }

        NX_LOGX(lm("client %1 -> %2 (timeout=%3)")
            .arg(socket).arg(peer).arg(timeout), cl_logDEBUG1);

        connectClient(socket, peer);
    }

    void connectClient(AbstractStreamSocket* socket, const SocketAddress& peer)
    {
        const auto delay = 500 * utils::TestOptions::timeoutMultiplier();
        if (auto address = m_addressBinder.random(peer))
        {
            NX_LOGX(lm("connect %1 -> %2").arg(socket).arg(*address), cl_logDEBUG2);
            socket->connectAsync(
                address.get(),
                [=](SystemError::ErrorCode code)
                {
                    if (code == SystemError::noError)
                        return readOnClient(socket, peer);

                    socket->registerTimer(
                        delay, [=](){ connectClient(socket, peer); });
                });
        }
        else
        {
            socket->registerTimer(
                nx::utils::random::number<int>(1, delay),
                [=]()
                {
                    if (auto address = m_addressBinder.random(peer))
                        return connectClient(socket, peer);

                    NX_LOGX(lm("indicate %1 -> %2").arg(socket).arg(peer), cl_logDEBUG2);
                    emitIndication(peer);
                    socket->registerTimer(
                        delay, [=](){ connectClient(socket, peer); });
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
            stun::extension::indications::connectionRequested));
        event.serialize(&message);

        m_stunClient->emulateIndication(message);
    }

    void readOnClient(AbstractStreamSocket* socketPtr, const SocketAddress& peer)
    {
        auto buffer = std::make_shared<Buffer>();
        buffer->reserve(network::test::kTestMessage.size() + 1);
        socketPtr->readSomeAsync(
            buffer.get(),
            [=](SystemError::ErrorCode code, size_t size)
            {
                std::unique_ptr<AbstractStreamSocket> socket;
                {
                    QnMutexLocker lock(&m_mutex);
                    auto socketIt = m_connectSockets.find(socketPtr);
                    if (socketIt != m_connectSockets.end())
                    {
                        socket = std::move(socketIt->second);
                        m_connectSockets.erase(socketIt);
                    }
                }

                if (code != SystemError::noError ||
                    size != static_cast<size_t>(
                        network::test::kTestMessage.size()))
                {
                    NX_LOGX(lm("read %1 failed (size=%2): %3")
                        .args(socket, size, SystemError::toString(code)), cl_logDEBUG2);

                    return startClient(peer);
                }

                NX_LOGX(lm("read %1 successed").arg(socket), cl_logDEBUG2);
                EXPECT_EQ(*buffer, network::test::kTestMessage);
                m_connectedResults.push(code);
            });
    }

    virtual void TearDown() override
    {
        const auto kTotalConnects = kThreadCount * kClientCount * kPeerCount;
        for (size_t i = 0; i < kTotalConnects; ++i)
            m_connectedResults.pop();

        m_server->pleaseStopSync();
        for (auto& thread : m_threads)
            thread.join();

        for (auto& socket: m_connectSockets)
            socket.second->pleaseStopSync();

        EXPECT_GE(m_acceptedSockets.size(), kTotalConnects);
        for (auto& socket : m_acceptedSockets)
            socket->pleaseStopSync();

        TunnelAcceptorFactory::instance().setCustomFunc(
            std::move(m_tunnelAcceptorFactoryFuncBak));
    }

    network::test::AddressBinder m_addressBinder;
    std::set<SocketAddress> m_boundPeers;

    std::unique_ptr<hpm::api::AbstractMediatorConnector> m_mediatorConnector;
    std::shared_ptr<stun::test::AsyncClientMock> m_stunClient;
    std::unique_ptr<AbstractStreamServerSocket> m_server;
    utils::TestSyncQueue<SystemError::ErrorCode> m_connectedResults;
    std::vector<nx::utils::thread> m_threads;

    QnMutex m_mutex;
    std::vector<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
    std::map<void*, std::unique_ptr<AbstractStreamSocket>> m_connectSockets;
    TunnelAcceptorFactory::Function m_tunnelAcceptorFactoryFuncBak;
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

//-------------------------------------------------------------------------------------------------

class CloudServerSocket:
    public ::testing::Test
{
public:
    CloudServerSocket()
    {
        stun::AbstractAsyncClient::Settings stunClientSettings;
        stunClientSettings.reconnectPolicy =
            nx::network::RetryPolicy(
                nx::network::RetryPolicy::kInfiniteRetries,
                std::chrono::milliseconds(0),
                nx::network::RetryPolicy::kDefaultDelayMultiplier,
                std::chrono::minutes(1));
        nx::hpm::api::MediatorConnector::setStunClientSettings(stunClientSettings);

        SocketGlobalsHolder::instance()->reinitialize();
    }

    ~CloudServerSocket()
    {
        if (m_cloudServerSocket)
            destroyServerSocket();
    }

protected:
    hpm::MediatorFunctionalTest m_mediator;
    hpm::AbstractCloudDataProvider::System system;
    std::unique_ptr<hpm::MediaServerEmulator> server;

    void startMediatorAndRegister()
    {
        ASSERT_TRUE(m_mediator.startAndWaitUntilStarted());
        system = m_mediator.addRandomSystem();
        server = m_mediator.addRandomServer(
            system, boost::none, hpm::ServerTweak::noBindEndpoint);

        ASSERT_NE(nullptr, server);
        SocketGlobals::cloud().mediatorConnector().setSystemCredentials(
            nx::hpm::api::SystemCredentials(
                system.id,
                server->serverId(),
                system.authKey));

        SocketGlobals::cloud().mediatorConnector().mockupMediatorUrl(
            nx::network::url::Builder().setScheme("stun")
                .setEndpoint(m_mediator.stunEndpoint()));
        SocketGlobals::cloud().mediatorConnector().enable(true);
    }

    void givenInitializedServerSocket()
    {
        m_cloudServerSocket = std::make_unique<cloud::CloudServerSocket>(
            &nx::network::SocketGlobals::cloud().mediatorConnector());
        ASSERT_EQ(
            hpm::api::ResultCode::ok,
            m_cloudServerSocket->registerOnMediatorSync());
        ASSERT_TRUE(m_cloudServerSocket->listen(128));
        m_cloudServerSocket->moveToListeningState();
    }

    void whenMediatorIsRestarted()
    {
        ASSERT_TRUE(m_mediator.restart());
    }

    void destroyServerSocket()
    {
        if (!m_cloudServerSocket)
            return;
        m_cloudServerSocket->pleaseStopSync();
        m_cloudServerSocket.reset();
    }

    cloud::CloudServerSocket& cloudServerSocket()
    {
        return *m_cloudServerSocket;
    }

private:
    std::unique_ptr<cloud::CloudServerSocket> m_cloudServerSocket;
};

TEST_F(CloudServerSocket, reconnect)
{
    startMediatorAndRegister();
    hpm::api::SystemCredentials currentCredentials =
        *nx::network::SocketGlobals::cloud().mediatorConnector().getSystemCredentials();

    auto system2 = m_mediator.addRandomSystem();
    auto server2 = m_mediator.addRandomServer(
        system, boost::none, hpm::ServerTweak::noBindEndpoint);

    hpm::api::SystemCredentials otherCredentials;
    otherCredentials.systemId = system2.id;
    otherCredentials.key = system2.authKey;
    otherCredentials.serverId = server2->serverId();

    for (int i = 0; i < 17; ++i)
    {
        givenInitializedServerSocket();
        destroyServerSocket();

        // Breaking connection to mediator.
        nx::utils::promise<void> cloudCredentialsModified;
        nx::network::SocketGlobals::cloud().mediatorConnector().post(
            [&cloudCredentialsModified, &otherCredentials]()
            {
                nx::network::SocketGlobals::cloud().mediatorConnector()
                    .setSystemCredentials(otherCredentials);
                cloudCredentialsModified.set_value();
            });
        cloudCredentialsModified.get_future().wait();
        std::swap(currentCredentials, otherCredentials);
    }
}

TEST_F(CloudServerSocket, serverChecksConnectionState)
{
    const KeepAliveOptions kKeepAliveOptions(std::chrono::seconds(1), std::chrono::seconds(1), 1);
    m_mediator.addArg(
        "-general/cloudConnectOptions", "serverChecksConnectionState");
    m_mediator.addArg(
        "-stun/keepAliveOptions", kKeepAliveOptions.toString().toStdString().c_str());
    startMediatorAndRegister();

    givenInitializedServerSocket();

    const auto peerPool = m_mediator.moduleInstance()->impl()->listeningPeerPool();
    {
        const auto peer = peerPool->findAndLockPeerDataByHostName(server->fullName());
        ASSERT_TRUE((bool) peer);
        ASSERT_TRUE(peer->value().isListening);

        // Drop listening state, e.g. because of some bug.
        const auto peerData = const_cast<hpm::ListeningPeerData*>(&peer->value());
        peerData->isListening = false;
    }

    // After some time server is supposed to detect an error and restore listening state.
    std::this_thread::sleep_for(kKeepAliveOptions.maxDelay() * 2);
    {
        const auto peer = peerPool->findAndLockPeerDataByHostName(server->fullName());
        ASSERT_TRUE((bool) peer);
        ASSERT_TRUE(peer->value().isListening);
    }
}

//-------------------------------------------------------------------------------------------------
// CloudServerSocketMultipleAcceptors

class CloudServerSocketMultipleAcceptors:
    public CloudServerSocket
{
public:
    CloudServerSocketMultipleAcceptors()
    {
        using namespace std::placeholders;

        startMediatorAndRegister();
        m_factoryFuncBak = cloud::CustomAcceptorFactory::instance().setCustomFunc(
            std::bind(&CloudServerSocketMultipleAcceptors::customAcceptorFactoryFunc, this,
                _1, _2));
    }

    ~CloudServerSocketMultipleAcceptors()
    {
        destroyServerSocket();

        cloud::CustomAcceptorFactory::instance().setCustomFunc(
            std::move(m_factoryFuncBak));
    }

protected:
    void givenAcceptingServerSocket()
    {
        givenInitializedServerSocket();
        whenAcceptIsInvoked();
        thenEveryAcceptorIsInvoked();
    }

    void whenAcceptIsInvoked()
    {
        using namespace std::placeholders;

        cloudServerSocket().acceptAsync(
            std::bind(&CloudServerSocketMultipleAcceptors::onAcceptCompletion, this, _1, _2));
    }

    void whenOneAcceptorReturnsSocket()
    {
        auto acceptor = nx::utils::random::choice(m_acceptorsCreated);
        acceptor->addReadyConnection(std::make_unique<network::TCPSocket>(AF_INET));
    }

    void thenCustomAcceptorsAreRemoved()
    {
        m_removedAcceptorsQueue.pop();
    }

    void thenEveryAcceptorIsInvoked()
    {
        for (;;)
        {
            bool isEveryAcceptorInvoked = true;
            for (const auto& acceptor: m_acceptorsCreated)
                isEveryAcceptorInvoked &= acceptor->isAsyncAcceptInProgress();
            if (isEveryAcceptorInvoked)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void thenEveryAcceptorIsCancelled()
    {
        for (;;)
        {
            bool isAnyAcceptorNotInvoked = false;
            for (const auto& acceptor: m_acceptorsCreated)
                isAnyAcceptorNotInvoked |= acceptor->isAsyncAcceptInProgress();
            if (!isAnyAcceptorNotInvoked)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void assertCloudCredentialsHaveBeenProvidedToCustomAcceptors()
    {
        ASSERT_TRUE(static_cast<bool>(m_providedCloudCredentials));
        ASSERT_EQ(
            SocketGlobals::cloud().mediatorConnector().getSystemCredentials(),
            *m_providedCloudCredentials);
    }

private:
    cloud::CustomAcceptorFactory::Function m_factoryFuncBak;
    utils::SyncQueue<network::test::AcceptorStub*> m_removedAcceptorsQueue;
    std::vector<network::test::AcceptorStub*> m_acceptorsCreated;
    boost::optional<hpm::api::SystemCredentials> m_providedCloudCredentials;

    std::vector<std::unique_ptr<AbstractConnectionAcceptor>> customAcceptorFactoryFunc(
        const hpm::api::SystemCredentials& credentials,
        const hpm::api::ListenResponse& /*response*/)
    {
        const int acceptorCount = 7;

        m_providedCloudCredentials = credentials;

        std::vector<std::unique_ptr<AbstractConnectionAcceptor>> acceptors;
        for (int i = 0; i < acceptorCount; ++i)
        {
            auto acceptor = std::make_unique<network::test::AcceptorStub>();
            acceptor->setRemovedAcceptorsQueue(&m_removedAcceptorsQueue);
            m_acceptorsCreated.push_back(acceptor.get());
            acceptors.push_back(std::move(acceptor));
        }
        return acceptors;
    }

    void onAcceptCompletion(
        SystemError::ErrorCode /*sysErrorCode*/,
        std::unique_ptr<AbstractStreamSocket> /*connection*/)
    {
    }
};

TEST_F(CloudServerSocketMultipleAcceptors, restoring_connection_to_the_cloud)
{
    givenInitializedServerSocket();
    whenMediatorIsRestarted();
    thenCustomAcceptorsAreRemoved();
}

TEST_F(CloudServerSocketMultipleAcceptors, all_acceptors_are_used)
{
    givenInitializedServerSocket();
    whenAcceptIsInvoked();
    thenEveryAcceptorIsInvoked();
}

TEST_F(CloudServerSocketMultipleAcceptors, all_acceptors_are_cancelled)
{
    givenAcceptingServerSocket();
    whenOneAcceptorReturnsSocket();
    thenEveryAcceptorIsCancelled();
}

TEST_F(CloudServerSocketMultipleAcceptors, provides_cloud_credentials_to_all_acceptors)
{
    givenInitializedServerSocket();
    assertCloudCredentialsHaveBeenProvidedToCustomAcceptors();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
