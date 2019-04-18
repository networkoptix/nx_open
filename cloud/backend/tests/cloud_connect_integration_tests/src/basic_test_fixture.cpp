#include "basic_test_fixture.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/socket_global.h>
#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/mediator/listening_peer_db.h>
#include <nx/cloud/mediator/listening_peer_pool.h>
#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>
#include <nx/cloud/relaying/listening_peer_pool.h>

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
        std::atomic<int>* blockedConnectionNumber)
        :
        m_connectionNumber(connectionNumber),
        m_blockedConnectionNumber(blockedConnectionNumber)
    {
    }

    virtual int write(const void* data, size_t count) override
    {
        if (m_connectionNumber <= m_blockedConnectionNumber->load())
            return 0;

        return m_outputStream->write(data, count);
    }

private:
    const int m_connectionNumber;
    std::atomic<int>* m_blockedConnectionNumber = nullptr;
};

//-------------------------------------------------------------------------------------------------

using namespace nx::cloud::relay;

static const char* const kCloudModulesXmlPath = "/cloud_modules.xml";

void PredefinedCredentialsProvider::setCredentials(
    hpm::api::SystemCredentials cloudSystemCredentials)
{
    m_cloudSystemCredentials = std::move(cloudSystemCredentials);
}

std::optional<hpm::api::SystemCredentials>
    PredefinedCredentialsProvider::getSystemCredentials() const
{
    return m_cloudSystemCredentials;
}

//-------------------------------------------------------------------------------------------------

void MemoryRemoteRelayPeerPool::addPeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_relayTest->peerAdded(domainName);
    return handler(true);
}

bool MemoryRemoteRelayPeerPool::connectToDb()
{
    return true;
}

bool MemoryRemoteRelayPeerPool::isConnected() const
{
    return true;
}

void MemoryRemoteRelayPeerPool::removePeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_relayTest->peerRemoved(domainName);
    return handler(true);
}

void MemoryRemoteRelayPeerPool::findRelayByDomain(
    const std::string& /*domainName*/,
    nx::utils::MoveOnlyFunc<void(std::string /*relay hostname/ip*/)> handler) const
{
    auto redirectToEndpoint = m_relayTest->relayInstanceEndpoint(0).toStdString();
    return handler(std::move(redirectToEndpoint));
}

//-------------------------------------------------------------------------------------------------


MediatorConnectorCluster::Context::Context(
    nx::hpm::MediatorFunctionalTest& mediator,
    const QString& cloudHost)
    :
    mediator(mediator),
    connector(cloudHost.toStdString())
{
}

MediatorConnectorCluster::Context& MediatorConnectorCluster::addContext(
    const std::vector<const char*> args,
    int flags,
    const QString& testDir,
    const QString& cloudHost)
{
    auto context = std::make_unique<Context>(
        m_cluster.addMediator(args, flags, testDir),
        !cloudHost.isEmpty() ? cloudHost : AppInfo::defaultCloudHostName());

    m_mediators.emplace_back(std::move(context));
    return *m_mediators.back();
}

MediatorConnectorCluster::Context& MediatorConnectorCluster::context(int index)
{
    return *m_mediators[index];
}

const nx::hpm::test::MediatorCluster& MediatorConnectorCluster::cluster() const
{
    return m_cluster;
}

//-------------------------------------------------------------------------------------------------

BasicTestFixture::BasicTestFixture(
    int relayCount,
    std::optional<std::chrono::seconds> disconnectedPeerTimeout)
    :
    base_type("cloud_connect_integration"),
    m_staticMsgBody("Hello, hren!"),
    m_unfinishedRequestsLeft(0),
    m_relayCount(relayCount),
    m_disconnectedPeerTimeout(disconnectedPeerTimeout)
{
    addMediator();
}

BasicTestFixture::~BasicTestFixture()
{
    std::list<std::unique_ptr<nx::network::http::AsyncClient>> httpClients;
    {
        QnMutexLocker lock(&m_mutex);
        httpClients.swap(m_httpClients);
    }

    for (const auto& httpClient : httpClients)
        httpClient->pleaseStopSync();

    m_httpServer.reset();
}

void BasicTestFixture::setUpPublicIpFactoryFunc()
{
    auto discoverFunc = []() {return network::HostAddress("127.0.0.1"); };
    controller::PublicIpDiscoveryService::setDiscoverFunc(discoverFunc);
}

void BasicTestFixture::setUpRemoteRelayPeerPoolFactoryFunc()
{
    using namespace nx::cloud::relay::model;
    auto createRemoteRelayPeerPoolFunc =
        [this](const conf::Settings&)
        {
            return std::make_unique<MemoryRemoteRelayPeerPool>(this);
        };
    RemoteRelayPeerPoolFactory::instance().setCustomFunc(createRemoteRelayPeerPoolFunc);
}

void BasicTestFixture::setInitFlags(int flags)
{
    m_initFlags = flags;
}

network::SocketAddress BasicTestFixture::relayInstanceEndpoint(RelayPtrList::size_type index) const
{
    return m_relays[index]->moduleInstance()->httpEndpoints()[0];
}

void BasicTestFixture::addRelayStartupArgument(
    const std::string& name,
    const std::string& value)
{
    m_relayStartupArguments.push_back({name, value});
}

void BasicTestFixture::SetUp()
{
    setUpPublicIpFactoryFunc();
    setUpRemoteRelayPeerPoolFactoryFunc();
    startRelays();

    ASSERT_EQ(m_relayCount, m_relays.size());

    startMediator();

    startCloudModulesXmlProvider();
}

//-------------------------------------------------------------------------------------------------
// Mediator.

void BasicTestFixture::addMediator()
{
    auto& mediatorContext = m_mediatorCluster.addContext({
            "-stun/addrToListenList", "127.0.0.1:0",
            "-http/addrToListenList", "127.0.0.1:0"
        },
        nx::hpm::MediatorFunctionalTest::allFlags &
            ~nx::hpm::MediatorFunctionalTest::initializeConnectivity,
        testDataDir() + "/mediator");
    mediatorContext.mediator.setUseProxy(true);
}

int BasicTestFixture::mediatorCount() const
{
    return m_mediatorCluster.cluster().size();
}

void BasicTestFixture::startMediator(int index)
{
    if (!m_relays.empty())
        mediator(index).addArg("-trafficRelay/urls", relayUrl().toStdString().c_str());

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

void BasicTestFixture::restartMediator(int index)
{
    mediator(index).restart();
}

const nx::hpm::test::MediatorCluster& BasicTestFixture::mediatorCluster() const
{
    return m_mediatorCluster.cluster();
}

nx::hpm::MediatorFunctionalTest& BasicTestFixture::mediator(int index)
{
    return m_mediatorCluster.context(index).mediator;
}

MediatorConnectorCluster::Context& BasicTestFixture::mediatorContext(int index)
{
    return m_mediatorCluster.context(index);
}

void BasicTestFixture::setMediatorApiProtocol(MediatorApiProtocol mediatorApiProtocol)
{
    m_mediatorApiProtocol = mediatorApiProtocol;
}

void BasicTestFixture::configureProxyBeforeMediator()
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

void BasicTestFixture::blockDownTrafficThroughExistingConnectionsToMediator()
{
    m_proxyBeforeMediator->blockedConnectionNumber.store(
        m_proxyBeforeMediator->lastConnectionNumber.load());
}

//-------------------------------------------------------------------------------------------------
// Module URL provider.

void BasicTestFixture::startCloudModulesXmlProvider()
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

void BasicTestFixture::registerServicesInModuleProvider()
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

void BasicTestFixture::setRelayCount(int count)
{
    m_relayCount = count;
}

void BasicTestFixture::startRelays()
{
    for (int i = 0; i < m_relayCount; ++i)
        startRelay(i);
}

void BasicTestFixture::startRelay(int index)
{
    auto newRelay = std::make_unique<Relay>();

    std::string endpointString = "127.0.0.1:0";
    newRelay->addArg("-http/listenOn", endpointString.c_str());

    const auto dataDirString = lm("%1/relay_%2").args(testDataDir(), index).toStdString();
    newRelay->addArg("-dataDir", dataDirString.c_str());

    for (const auto& argument : m_relayStartupArguments)
    {
        newRelay->removeArgByName(argument.name.c_str());
        newRelay->addArg(
            lm("--%1=%2").args(argument.name, argument.value).toStdString().c_str());
    }

    if ((bool)m_disconnectedPeerTimeout)
    {
        newRelay->addArg(
            "-listeningPeer/disconnectedPeerTimeout",
            std::to_string(m_disconnectedPeerTimeout->count()).c_str());
    }

    ASSERT_TRUE(newRelay->startAndWaitUntilStarted());
    m_relays.push_back(std::move(newRelay));
}

nx::cloud::relay::test::Launcher& BasicTestFixture::trafficRelay()
{
    return *m_relays[0];
}

nx::utils::Url BasicTestFixture::relayUrl(int relayNum) const
{
    NX_ASSERT(relayNum < (int)m_relays.size());

    const auto& httpEndpoints = m_relays[relayNum]->moduleInstance()->httpEndpoints();
    const auto& httpsEndpoints = m_relays[relayNum]->moduleInstance()->httpsEndpoints();

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
    else
    {
        return nx::utils::Url();
    }
}

//-------------------------------------------------------------------------------------------------
// Listening server.

void BasicTestFixture::startServer(int mediatorIndex)
{
    auto cloudSystemCredentials = mediator(mediatorIndex).addRandomSystem();

    m_cloudSystemCredentials.systemId = cloudSystemCredentials.id;
    m_cloudSystemCredentials.serverId = QnUuid::createUuid().toSimpleByteArray();
    m_cloudSystemCredentials.key = cloudSystemCredentials.authKey;

    mediatorContext(mediatorIndex).connector.setSystemCredentials(m_cloudSystemCredentials);

    startHttpServer(&mediatorContext(mediatorIndex).connector);
}

void BasicTestFixture::stopServer()
{
    m_httpServer.reset();
}

std::string BasicTestFixture::serverSocketCloudAddress() const
{
    return m_cloudSystemCredentials.systemId.toStdString();
}

const hpm::api::SystemCredentials& BasicTestFixture::cloudSystemCredentials() const
{
    return m_cloudSystemCredentials;
}

void BasicTestFixture::setRemotePeerName(const std::string& remotePeerName)
{
    m_remotePeerName = remotePeerName;
}

//-------------------------------------------------------------------------------------------------

void BasicTestFixture::stopCloud()
{
    m_cloudModulesXmlProvider.httpMessageDispatcher().clear();
    mediator().stop();
    m_relays.clear();
}

void BasicTestFixture::startCloud()
{
    startRelays();
    startMediator();
    registerServicesInModuleProvider();
}

//-------------------------------------------------------------------------------------------------
// Validation functions.

void BasicTestFixture::assertCloudConnectionCanBeEstablished()
{
    ASSERT_EQ(SystemError::noError, tryEstablishCloudConnection());
}

void BasicTestFixture::waitUntilCloudConnectionCanBeEstablished()
{
    while (tryEstablishCloudConnection() != SystemError::noError)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

SystemError::ErrorCode BasicTestFixture::tryEstablishCloudConnection()
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

void BasicTestFixture::startExchangingFixedData()
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

void BasicTestFixture::assertDataHasBeenExchangedCorrectly()
{
    while (m_unfinishedRequestsLeft > 0)
    {
        const auto result = m_httpRequestResults.pop();
        ASSERT_EQ(nx::network::http::StatusCode::ok, result.statusCode);
        ASSERT_EQ(m_expectedMsgBody, result.msgBody);
        --m_unfinishedRequestsLeft;
    }
}

nx::hpm::MediatorEndpoint BasicTestFixture::waitUntilServerIsRegisteredOnMediator()
{
    std::string systemId = m_cloudSystemCredentials.systemId.toStdString();
    auto endpoint = m_mediatorCluster.cluster().lookupMediatorEndpoint(systemId);
    while (!endpoint)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        endpoint = m_mediatorCluster.cluster().lookupMediatorEndpoint(systemId);
    }
    return *endpoint;
}

void BasicTestFixture::waitUntilServerIsRegisteredOnTrafficRelay()
{
    waitForServerStatusOnRelay(ServerRelayStatus::registered);
}

void BasicTestFixture::waitUntilServerIsUnRegisteredOnTrafficRelay()
{
    waitForServerStatusOnRelay(ServerRelayStatus::unregistered);
}

void BasicTestFixture::waitForServerStatusOnRelay(ServerRelayStatus status)
{
    const auto& listeningPool = m_relays[0]->moduleInstance()->listeningPeerPool();
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

const std::unique_ptr<network::AbstractStreamSocket>& BasicTestFixture::clientSocket()
{
    return m_clientSocket;
}

void BasicTestFixture::initializeCloudModulesXmlWithDirectStunPort()
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

void BasicTestFixture::initializeCloudModulesXmlWithStunOverHttp()
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

void BasicTestFixture::startHttpServer(nx::hpm::api::MediatorConnector* connector)
{
    auto cloudServerSocket = std::make_unique<CloudServerSocket>(connector);

    m_httpServer = std::make_unique<nx::network::http::TestHttpServer>(std::move(cloudServerSocket));
    m_httpServer->registerStaticProcessor("/static", m_staticMsgBody, "text/plain");
    m_staticUrl = nx::utils::Url(lm("http://%1/static").arg(serverSocketCloudAddress()));

    ASSERT_TRUE(m_httpServer->bindAndListen());

    if (!m_relays.empty())
        waitUntilServerIsRegisteredOnTrafficRelay();
}

void BasicTestFixture::onHttpRequestDone(
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
