#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stream_proxy.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>
#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relay/test_support/traffic_relay_cluster.h>
#include <nx/cloud/relay/settings.h>
#include <nx/cloud/relaying/listening_peer_pool.h>
#include <nx/cloud/mediator/test_support/mediator_cluster.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class TrafficBlocker:
    public nx::utils::bstream::AbstractOutputConverter
{
public:
    TrafficBlocker(
        int connectionNumber,
        std::atomic<int>* blockedConnectionNumber);

    virtual int write(const void* data, size_t count) override;

private:
    const int m_connectionNumber;
    std::atomic<int>* m_blockedConnectionNumber = nullptr;
};

//-------------------------------------------------------------------------------------------------

class PredefinedCredentialsProvider:
    public hpm::api::AbstractCloudSystemCredentialsProvider
{
public:
    void setCredentials(hpm::api::SystemCredentials cloudSystemCredentials);

    virtual std::optional<hpm::api::SystemCredentials> getSystemCredentials() const override;

private:
    hpm::api::SystemCredentials m_cloudSystemCredentials;
};

//-------------------------------------------------------------------------------------------------

enum class MediatorApiProtocol
{
    stun,
    http
};

class PeerPoolEventHandler
{
public:
    virtual ~PeerPoolEventHandler() = default;

    virtual void peerAdded(const std::string& /*serverName*/) {}
    virtual void peerRemoved(const std::string& /*serverName*/) {}
};

class MemoryRemoteRelayPeerPool:
    public nx::cloud::relay::model::RemoteRelayPeerPool
{
    using base_type = nx::cloud::relay::model::RemoteRelayPeerPool;
public:
    MemoryRemoteRelayPeerPool(
        const nx::cloud::relay::conf::Settings& settings,
        PeerPoolEventHandler* relayTest);

    virtual void addPeer(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler) override;

    virtual void removePeer(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler) override;

private:
    PeerPoolEventHandler* m_relayTest = nullptr;
};

//-------------------------------------------------------------------------------------------------


class MediatorConnectorCluster
{
public:
    struct Context
    {
        nx::hpm::MediatorInstance& mediator;
        nx::hpm::api::MediatorConnector connector;

        Context(
            nx::hpm::MediatorInstance& mediator,
            const QString& cloudHost);

        Context(const Context&) = delete;
        Context& operator =(const Context&) = delete;
    };

public:
    MediatorConnectorCluster(const nx::utils::Url& discoveryServiceUrl);
    Context& addContext(const std::vector<const char*> args = {},
        int flags = nx::hpm::MediatorFunctionalTest::MediatorTestFlags::allFlags,
        const QString& testDir = QString(),
        const QString& cloudHost = QString());

    Context& context(int index);

    nx::hpm::test::MediatorCluster& cluster();
    const nx::hpm::test::MediatorCluster& cluster() const;

private:
    nx::hpm::test::MediatorCluster m_cluster;
    std::vector<std::unique_ptr<Context>> m_mediators;
};

//-------------------------------------------------------------------------------------------------

template<typename Base>
class BasicTestFixture:
    public Base,
    public PeerPoolEventHandler,
    public nx::utils::test::TestWithTemporaryDirectory
{
    using base_type = nx::utils::test::TestWithTemporaryDirectory;

    static_assert(std::is_base_of_v<::testing::Test, Base>);

public:
    enum Flag
    {
        doNotInitializeMediatorConnection = 1,
    };

    BasicTestFixture(
        int relayCount = 1,
        std::optional<std::chrono::seconds> disconnectedPeerTimeout = std::nullopt);
    ~BasicTestFixture();

    /**
     * @param flags Bitset of BasicTestFixture::Flag values.
     */
    void setInitFlags(int flags);
    network::SocketAddress relayInstanceEndpoint(int index) const;

    void addRelayStartupArgument(
        const std::string& name,
        const std::string& value);

protected:
    virtual void SetUp() override;
    virtual void TearDown() override;

    nx::cloud::discovery::test::DiscoveryServer& discoveryServer();

    /**
     * Mediator.
     */

    void addMediator();
    int mediatorCount() const;
    void startMediator(int index = 0);
    void restartMediator(int index = 0);
    const nx::hpm::test::MediatorCluster& mediatorCluster() const;
    nx::hpm::MediatorInstance& mediator(int index = 0);
    MediatorConnectorCluster::Context& mediatorContext(int index = 0);
    void setMediatorApiProtocol(MediatorApiProtocol mediatorApiProtocol);
    void configureProxyBeforeMediator();
    void blockDownTrafficThroughExistingConnectionsToMediator();

    /**
     * Module URL provider.
     */

    void startCloudModulesXmlProvider();
    void registerServicesInModuleProvider();

    /**
     * Relay.
     */

    void setRelayCount(int count);
    void startRelays();
    void startRelay(int index);
    nx::cloud::relay::test::TrafficRelay& trafficRelay(int index = 0);
    nx::utils::Url relayUrl(int relayNum = 0) const;
    std::string relayClusterId() const;
    int relayClusterSize() const;

    /**
     * Listening server.
     */

    /**
     * Starts the server on mediator(mediatorIndex).
     */
    void startServer(int mediatorIndex = 0);
    void stopServer();
    std::string serverSocketCloudAddress() const;
    const hpm::api::SystemCredentials& cloudSystemCredentials() const;
    void setRemotePeerName(const std::string& peerName);

    /**
     * Stops all Cloud services launched with corresponding functions.
     */
    void stopCloud();

    /**
     * Starts Cloud services as before invokation of stopCloud().
     */
    void startCloud();

    /**
     * Validation functions.
     */

    void assertCloudConnectionCanBeEstablished();
    void waitUntilCloudConnectionCanBeEstablished();
    SystemError::ErrorCode tryEstablishCloudConnection();

    void startExchangingFixedData();
    void assertDataHasBeenExchangedCorrectly();

    nx::hpm::MediatorEndpoint waitUntilServerIsRegisteredOnMediator();
    void waitUntilServerIsRegisteredOnTrafficRelay();
    void waitUntilServerIsUnRegisteredOnTrafficRelay();

    const std::unique_ptr<network::AbstractStreamSocket>& clientSocket();

private:
    friend class MemoryRemoteRelayPeerPool;

    struct HttpRequestResult
    {
        nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::undefined;
        nx::network::http::BufferType msgBody;
    };

    enum class ServerRelayStatus
    {
        registered,
        unregistered
    };

    struct StartupArgument
    {
        std::string name;
        std::string value;
    };

    struct ProxyContext
    {
        nx::network::SocketAddress endpoint;
        nx::network::StreamProxy proxy;
        std::atomic<int> lastConnectionNumber{0};
        std::atomic<int> blockedConnectionNumber{-1};
    };

    QnMutex m_mutex;
    const nx::network::http::BufferType m_staticMsgBody;
    nx::cloud::discovery::test::DiscoveryServer m_discoveryServer;
    std::string m_discoveryServiceUrl;
    std::unique_ptr<MediatorConnectorCluster> m_mediatorCluster;
    hpm::api::SystemCredentials m_cloudSystemCredentials;
    std::unique_ptr<nx::network::http::TestHttpServer> m_httpServer;
    nx::network::http::TestHttpServer m_cloudModulesXmlProvider;
    nx::utils::Url m_staticUrl;
    std::list<std::unique_ptr<nx::network::http::AsyncClient>> m_httpClients;
    std::atomic<int> m_unfinishedRequestsLeft{0};
    std::optional<std::string> m_remotePeerName;
    MediatorApiProtocol m_mediatorApiProtocol = MediatorApiProtocol::http;
    int m_initFlags = 0;
    std::vector<StartupArgument> m_relayStartupArguments;

    nx::utils::SyncQueue<HttpRequestResult> m_httpRequestResults;
    nx::network::http::BufferType m_expectedMsgBody;
    std::unique_ptr<network::AbstractStreamSocket> m_clientSocket;

    std::unique_ptr<ProxyContext> m_proxyBeforeMediator;

    std::unique_ptr<nx::cloud::relay::test::TrafficRelayCluster> m_relayCluster;
    int m_relayCount;
    std::optional<std::chrono::seconds> m_disconnectedPeerTimeout;

    void initializeCloudModulesXmlWithDirectStunPort();
    void initializeCloudModulesXmlWithStunOverHttp();
    void startHttpServer(nx::hpm::api::MediatorConnector* connector);
    void onHttpRequestDone(
        std::list<std::unique_ptr<nx::network::http::AsyncClient>>::iterator clientIter);
    void waitForServerStatusOnRelay(ServerRelayStatus status);

    void setUpRemoteRelayPeerPoolFactoryFunc();
    void setUpPublicIpFactoryFunc();
};

//-------------------------------------------------------------------------------------------------

template<typename Base>
BasicTestFixture<Base>::BasicTestFixture(
    int relayCount,
    std::optional<std::chrono::seconds> disconnectedPeerTimeout)
    :
    m_staticMsgBody("Hello, hren!"),
    m_relayCount(relayCount),
    m_disconnectedPeerTimeout(disconnectedPeerTimeout)
{
    NX_ASSERT(m_discoveryServer.bindAndListen());
    m_discoveryServiceUrl = m_discoveryServer.url().toStdString();

    m_mediatorCluster = std::make_unique<MediatorConnectorCluster>(m_discoveryServer.url());
    m_relayCluster =
        std::make_unique<nx::cloud::relay::test::TrafficRelayCluster>(m_discoveryServer.url());

    addMediator();
}

template<typename Base>
BasicTestFixture<Base>::~BasicTestFixture()
{
}

template<typename Base>
void BasicTestFixture<Base>::setUpPublicIpFactoryFunc()
{
    auto discoverFunc = []() {return network::HostAddress("127.0.0.1"); };
    nx::cloud::relay::controller::PublicIpDiscoveryService::setDiscoverFunc(discoverFunc);
}

template<typename Base>
void BasicTestFixture<Base>::setUpRemoteRelayPeerPoolFactoryFunc()
{
    using namespace nx::cloud::relay::model;
    auto createRemoteRelayPeerPoolFunc =
        [this](const nx::cloud::relay::conf::Settings& settings)
        {
            return std::make_unique<MemoryRemoteRelayPeerPool>(settings, this);
        };
    RemoteRelayPeerPoolFactory::instance().setCustomFunc(createRemoteRelayPeerPoolFunc);
}

template<typename Base>
void BasicTestFixture<Base>::setInitFlags(int flags)
{
    m_initFlags = flags;
}

template<typename Base>
network::SocketAddress BasicTestFixture<Base>::relayInstanceEndpoint(int index) const
{
    return m_relayCluster->relay(index).httpEndpoint(0);
}

template<typename Base>
void BasicTestFixture<Base>::addRelayStartupArgument(
    const std::string& name,
    const std::string& value)
{
    m_relayStartupArguments.push_back({name, value});
}

template<typename Base>
void BasicTestFixture<Base>::SetUp()
{
    setUpPublicIpFactoryFunc();
    setUpRemoteRelayPeerPoolFactoryFunc();
    startRelays();

    ASSERT_EQ(m_relayCount, m_relayCluster->size());

    startMediator();

    startCloudModulesXmlProvider();
}

template<typename Base>
void BasicTestFixture<Base>::TearDown()
{
    std::list<std::unique_ptr<nx::network::http::AsyncClient>> httpClients;
    {
        QnMutexLocker lock(&m_mutex);
        httpClients.swap(m_httpClients);
    }

    for (const auto& httpClient : httpClients)
        httpClient->pleaseStopSync();

    m_httpServer.reset();

    m_relayCluster.reset();

    m_mediatorCluster.reset();
}

template<typename Base>
nx::cloud::discovery::test::DiscoveryServer& BasicTestFixture<Base>::discoveryServer()
{
    return m_discoveryServer;
}

//-------------------------------------------------------------------------------------------------
// Mediator.

template<typename Base>
void BasicTestFixture<Base>::addMediator()
{
    auto& mediatorContext = m_mediatorCluster->addContext({
            "-stun/addrToListenList", "127.0.0.1:0",
            "-http/addrToListenList", "127.0.0.1:0",
            "-trafficRelay/clusterId", m_relayCluster->clusterId().c_str(),
            "-trafficRelay/cluster/discovery/enabled", "true",
            "-trafficRelay/cluster/discovery/discoveryServiceUrl", m_discoveryServiceUrl.c_str(),
            "-trafficRelay/cluster/discovery/roundTripPadding", "1ms",
            "-trafficRelay/cluster/discovery/registrationErrorDelay", "1ms",
            "-trafficRelay/cluster/discovery/onlineNodesRequestDelay", "1ms"
        },
        nx::hpm::MediatorInstance::allFlags &
            ~nx::hpm::MediatorInstance::initializeConnectivity,
        testDataDir() + QString("/mediator_%1").arg(mediatorCluster().size()));
    mediatorContext.mediator.setUseProxy(true);
}

template<typename Base>
int BasicTestFixture<Base>::mediatorCount() const
{
    return m_mediatorCluster->cluster().size();
}

template<typename Base>
void BasicTestFixture<Base>::startMediator(int index)
{
    if (!m_relayCluster->empty())
    {
        std::string relayUrls;
        for (int i = 0; i < m_relayCluster->size(); ++i)
            relayUrls.append(relayUrl(i).toStdString()).append(",");
        relayUrls.pop_back(); //< dropping final comma

        mediator(index).addArg("-trafficRelay/url", relayUrls.c_str());
    }

    ASSERT_TRUE(mediator(index).startAndWaitUntilStarted());

    auto& context = mediatorContext(index);
    context.connector.mockupMediatorAddress({
        context.mediator.httpUrl(),
        context.mediator.stunUdpEndpoint()});

    if (m_proxyBeforeMediator)
    {
        switch (m_mediatorApiProtocol)
        {
            case MediatorApiProtocol::stun:
                m_proxyBeforeMediator->proxy.setProxyDestination(mediator(index).stunTcpEndpoint());
                break;

            case MediatorApiProtocol::http:
                m_proxyBeforeMediator->proxy.setProxyDestination(mediator(index).httpEndpoint());
                break;
        }
    }
}

template<typename Base>
void BasicTestFixture<Base>::restartMediator(int index)
{
    mediator(index).restart();
}

template<typename Base>
const nx::hpm::test::MediatorCluster& BasicTestFixture<Base>::mediatorCluster() const
{
    return m_mediatorCluster->cluster();
}

template<typename Base>
nx::hpm::MediatorInstance& BasicTestFixture<Base>::mediator(int index)
{
    return m_mediatorCluster->context(index).mediator;
}

template<typename Base>
MediatorConnectorCluster::Context& BasicTestFixture<Base>::mediatorContext(int index)
{
    return m_mediatorCluster->context(index);
}

template<typename Base>
void BasicTestFixture<Base>::setMediatorApiProtocol(MediatorApiProtocol mediatorApiProtocol)
{
    m_mediatorApiProtocol = mediatorApiProtocol;
}

template<typename Base>
void BasicTestFixture<Base>::configureProxyBeforeMediator()
{
    m_proxyBeforeMediator = std::make_unique<ProxyContext>();

    auto listener = std::make_unique<nx::network::TCPServerSocket>(AF_INET);
    ASSERT_TRUE(listener->setNonBlockingMode(true));
    ASSERT_TRUE(listener->bind(nx::network::SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(listener->listen());
    m_proxyBeforeMediator->endpoint = listener->getLocalAddress();

    m_proxyBeforeMediator->proxy.setDownStreamConverterFactory(
        [proxyContext = m_proxyBeforeMediator.get()]()
        {
            return std::make_unique<TrafficBlocker>(
                ++proxyContext->lastConnectionNumber,
                &proxyContext->blockedConnectionNumber);
        });

    m_proxyBeforeMediator->proxy.startProxy(
        std::make_unique<nx::network::StreamServerSocketToAcceptorWrapper>(std::move(listener)),
        nx::network::SocketAddress());
}

template<typename Base>
void BasicTestFixture<Base>::blockDownTrafficThroughExistingConnectionsToMediator()
{
    m_proxyBeforeMediator->blockedConnectionNumber.store(
        m_proxyBeforeMediator->lastConnectionNumber.load());
}

//-------------------------------------------------------------------------------------------------
// Module URL provider.

static constexpr char kCloudModulesXmlPath[] = "/cloud_modules.xml";

template<typename Base>
void BasicTestFixture<Base>::startCloudModulesXmlProvider()
{
    ASSERT_TRUE(m_cloudModulesXmlProvider.bindAndListen());

    registerServicesInModuleProvider();

    if ((m_initFlags & doNotInitializeMediatorConnection) == 0)
    {
        SocketGlobals::cloud().mediatorConnector().mockupCloudModulesXmlUrl(
            nx::network::url::Builder().setScheme("http")
            .setEndpoint(m_cloudModulesXmlProvider.serverAddress())
            .setPath(kCloudModulesXmlPath));
    }
}

template<typename Base>
void BasicTestFixture<Base>::registerServicesInModuleProvider()
{
    switch (m_mediatorApiProtocol)
    {
        case MediatorApiProtocol::stun:
            initializeCloudModulesXmlWithDirectStunPort();
            break;

        case MediatorApiProtocol::http:
            initializeCloudModulesXmlWithStunOverHttp();
            break;
    }
}

//-------------------------------------------------------------------------------------------------
// Relay.

template<typename Base>
void BasicTestFixture<Base>::setRelayCount(int count)
{
    m_relayCount = count;
}

template<typename Base>
void BasicTestFixture<Base>::startRelays()
{
    for (int i = 0; i < m_relayCount; ++i)
        startRelay(i);
}

template<typename Base>
void BasicTestFixture<Base>::startRelay(int index)
{
    auto& newRelay = m_relayCluster->addRelay();

    const auto dataDirString = lm("%1/relay_%2").args(testDataDir(), index).toStdString();
    newRelay.addArg("-dataDir", dataDirString.c_str());

    for (const auto& argument : m_relayStartupArguments)
    {
        newRelay.removeArgByName(argument.name.c_str());
        newRelay.addArg(
            lm("--%1=%2").args(argument.name, argument.value).toStdString().c_str());
    }

    if ((bool)m_disconnectedPeerTimeout)
    {
        newRelay.addArg(
            "-listeningPeer/disconnectedPeerTimeout",
            std::to_string(m_disconnectedPeerTimeout->count()).c_str());
    }

    ASSERT_TRUE(newRelay.startAndWaitUntilStarted());
}

template<typename Base>
nx::cloud::relay::test::TrafficRelay& BasicTestFixture<Base>::trafficRelay(int index)
{
    return m_relayCluster->relay(index);
}

template<typename Base>
nx::utils::Url BasicTestFixture<Base>::relayUrl(int relayNum) const
{
    NX_ASSERT(relayNum < (int)m_relayCluster->size());

    const auto& httpEndpoints = m_relayCluster->relay(relayNum).moduleInstance()->httpEndpoints();
    const auto& httpsEndpoints = m_relayCluster->relay(relayNum).moduleInstance()->httpsEndpoints();

    if (!httpEndpoints.empty())
    {
        return network::url::Builder()
            .setScheme(network::http::kUrlSchemeName)
            .setEndpoint(httpEndpoints[0]);
    }
    else if (!httpsEndpoints.empty())
    {
        return network::url::Builder()
            .setScheme(network::http::kSecureUrlSchemeName)
            .setEndpoint(httpsEndpoints[0]);
    }

    return nx::utils::Url();
}

template<typename Base>
std::string BasicTestFixture<Base>::relayClusterId() const
{
    return m_relayCluster->clusterId();
}

template<typename Base>
int BasicTestFixture<Base>::relayClusterSize() const
{
    return m_relayCluster->size();
}

//-------------------------------------------------------------------------------------------------
// Listening server.

template<typename Base>
void BasicTestFixture<Base>::startServer(int mediatorIndex)
{
    auto cloudSystemCredentials = m_mediatorCluster->cluster().addRandomSystem();

    m_cloudSystemCredentials.systemId = cloudSystemCredentials.id;
    m_cloudSystemCredentials.serverId = QnUuid::createUuid().toSimpleByteArray();
    m_cloudSystemCredentials.key = cloudSystemCredentials.authKey;

    mediatorContext(mediatorIndex).connector.setSystemCredentials(m_cloudSystemCredentials);

    startHttpServer(&mediatorContext(mediatorIndex).connector);
}

template<typename Base>
void BasicTestFixture<Base>::stopServer()
{
    m_httpServer.reset();
}

template<typename Base>
std::string BasicTestFixture<Base>::serverSocketCloudAddress() const
{
    return m_cloudSystemCredentials.systemId.toStdString();
}

template<typename Base>
const hpm::api::SystemCredentials& BasicTestFixture<Base>::cloudSystemCredentials() const
{
    return m_cloudSystemCredentials;
}

template<typename Base>
void BasicTestFixture<Base>::setRemotePeerName(const std::string& remotePeerName)
{
    m_remotePeerName = remotePeerName;
}

//-------------------------------------------------------------------------------------------------

template<typename Base>
void BasicTestFixture<Base>::stopCloud()
{
    m_cloudModulesXmlProvider.httpMessageDispatcher().clear();
    mediator().stop();
    m_relayCluster->clear();
}

template<typename Base>
void BasicTestFixture<Base>::startCloud()
{
    startRelays();
    startMediator();
    registerServicesInModuleProvider();
}

//-------------------------------------------------------------------------------------------------
// Validation functions.

template<typename Base>
void BasicTestFixture<Base>::assertCloudConnectionCanBeEstablished()
{
    ASSERT_EQ(SystemError::noError, tryEstablishCloudConnection());
}

template<typename Base>
void BasicTestFixture<Base>::waitUntilCloudConnectionCanBeEstablished()
{
    while (tryEstablishCloudConnection() != SystemError::noError)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

template<typename Base>
SystemError::ErrorCode BasicTestFixture<Base>::tryEstablishCloudConnection()
{
    m_clientSocket = SocketFactory::createStreamSocket();
    auto clientSocketGuard =
        nx::utils::makeScopeGuard([this]() { m_clientSocket->pleaseStopSync(); });
    if (!m_clientSocket->setNonBlockingMode(true))
        return SystemError::getLastOSErrorCode();

    std::string targetAddress =
        m_remotePeerName
        ? *m_remotePeerName
        : serverSocketCloudAddress();

    nx::utils::promise<SystemError::ErrorCode> done;
    m_clientSocket->connectAsync(
        targetAddress,
        [&done](SystemError::ErrorCode sysErrorCode)
        {
            done.set_value(sysErrorCode);
        });
    return done.get_future().get();
}

template<typename Base>
void BasicTestFixture<Base>::startExchangingFixedData()
{
    QnMutexLocker lock(&m_mutex);

    m_httpClients.push_back(std::make_unique<nx::network::http::AsyncClient>());
    m_httpClients.back()->setSendTimeout(kNoTimeout);
    m_httpClients.back()->setResponseReadTimeout(kNoTimeout);
    m_httpClients.back()->setMessageBodyReadTimeout(kNoTimeout);

    ++m_unfinishedRequestsLeft;

    m_expectedMsgBody = m_staticMsgBody;
    m_httpClients.back()->doGet(
        m_staticUrl,
        std::bind(&BasicTestFixture::onHttpRequestDone, this, --m_httpClients.end()));
}

template<typename Base>
void BasicTestFixture<Base>::assertDataHasBeenExchangedCorrectly()
{
    while (m_unfinishedRequestsLeft > 0)
    {
        const auto result = m_httpRequestResults.pop();
        ASSERT_EQ(nx::network::http::StatusCode::ok, result.statusCode);
        ASSERT_EQ(m_expectedMsgBody, result.msgBody);
        --m_unfinishedRequestsLeft;
    }
}

template<typename Base>
nx::hpm::MediatorEndpoint BasicTestFixture<Base>::waitUntilServerIsRegisteredOnMediator()
{
    std::string systemId = m_cloudSystemCredentials.systemId.toStdString();
    auto endpoint = m_mediatorCluster->cluster().lookupMediatorEndpoint(systemId);
    while (!endpoint)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        endpoint = m_mediatorCluster->cluster().lookupMediatorEndpoint(systemId);
    }
    return *endpoint;
}

template<typename Base>
void BasicTestFixture<Base>::waitUntilServerIsRegisteredOnTrafficRelay()
{
    waitForServerStatusOnRelay(ServerRelayStatus::registered);
}

template<typename Base>
void BasicTestFixture<Base>::waitUntilServerIsUnRegisteredOnTrafficRelay()
{
    waitForServerStatusOnRelay(ServerRelayStatus::unregistered);
}

template<typename Base>
void BasicTestFixture<Base>::waitForServerStatusOnRelay(ServerRelayStatus status)
{
    const auto& listeningPool = m_relayCluster->relay(0).moduleInstance()->listeningPeerPool();
    auto serverId = m_cloudSystemCredentials.systemId.toStdString();
    auto getServerStatus = [&]() { return listeningPool.isPeerOnline(serverId); };

    auto shouldWaitMore =
        [&]()
        {
            return
                (status == ServerRelayStatus::registered && !getServerStatus())
                || (status == ServerRelayStatus::unregistered && getServerStatus());
        };

    while (shouldWaitMore())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

template<typename Base>
const std::unique_ptr<network::AbstractStreamSocket>& BasicTestFixture<Base>::clientSocket()
{
    return m_clientSocket;
}

template<typename Base>
void BasicTestFixture<Base>::initializeCloudModulesXmlWithDirectStunPort()
{
    static const char* const kCloudModulesXmlTemplate = R"xml(
        <sequence>
            <set resName="hpm.tcpUrl" resValue="stun://%1"/>
            <set resName="hpm.udpUrl" resValue="stun://%2"/>
        </sequence>
    )xml";

    m_cloudModulesXmlProvider.registerStaticProcessor(
        kCloudModulesXmlPath,
        lm(kCloudModulesXmlTemplate).args(
            m_proxyBeforeMediator ? m_proxyBeforeMediator->endpoint : mediator().stunTcpEndpoint(),
            mediator().stunUdpEndpoint()).toUtf8(),
        "application/xml");
}

template<typename Base>
void BasicTestFixture<Base>::initializeCloudModulesXmlWithStunOverHttp()
{
    static const char* kCloudModulesXmlTemplate = R"xml(
        <sequence>
            <set resName="hpm.tcpUrl" resValue="http://%1%2"/>
            <set resName="hpm.udpUrl" resValue="stun://%3"/>
        </sequence>
    )xml";

    m_cloudModulesXmlProvider.registerStaticProcessor(
        kCloudModulesXmlPath,
        lm(kCloudModulesXmlTemplate)
            .args(m_proxyBeforeMediator ? m_proxyBeforeMediator->endpoint : mediator().httpEndpoint(),
                nx::hpm::api::kMediatorApiPrefix, mediator().stunUdpEndpoint()).toUtf8(),
        "application/xml");
}

template<typename Base>
void BasicTestFixture<Base>::startHttpServer(nx::hpm::api::MediatorConnector* connector)
{
    auto cloudServerSocket = std::make_unique<CloudServerSocket>(connector);

    m_httpServer = std::make_unique<nx::network::http::TestHttpServer>(std::move(cloudServerSocket));
    m_httpServer->registerStaticProcessor("/static", m_staticMsgBody, "text/plain");
    m_staticUrl = nx::utils::Url(lm("http://%1/static").arg(serverSocketCloudAddress()));

    ASSERT_TRUE(m_httpServer->bindAndListen());

    if (!m_relayCluster->empty())
        waitUntilServerIsRegisteredOnTrafficRelay();
}

template<typename Base>
void BasicTestFixture<Base>::onHttpRequestDone(
    std::list<std::unique_ptr<nx::network::http::AsyncClient>>::iterator clientIter)
{
    QnMutexLocker lock(&m_mutex);

    if (m_httpClients.empty())
        return; //< Most likely, current object is being destructed.

    auto httpClient = std::move(*clientIter);
    m_httpClients.erase(clientIter);

    HttpRequestResult result;
    if (httpClient->response())
    {
        result.statusCode =
            (nx::network::http::StatusCode::Value)httpClient->response()->statusLine.statusCode;
        result.msgBody = httpClient->fetchMessageBodyBuffer();
    }

    m_httpRequestResults.push(std::move(result));
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
