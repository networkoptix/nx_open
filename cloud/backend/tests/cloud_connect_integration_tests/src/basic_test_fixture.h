#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stream_proxy.h>
#include <nx/network/stun/async_client.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relay/test_support/traffic_relay_cluster.h>
#include <nx/cloud/relay/settings.h>
#include <nx/cloud/mediator/test_support/mediator_cluster.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

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

class BasicTestFixture;

class MemoryRemoteRelayPeerPool:
    public nx::cloud::relay::model::RemoteRelayPeerPool
{
    using base_type = nx::cloud::relay::model::RemoteRelayPeerPool;
public:
    MemoryRemoteRelayPeerPool(
        const nx::cloud::relay::conf::Settings& settings,
        BasicTestFixture* relayTest);

    virtual void addPeer(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler) override;

    virtual void removePeer(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler) override;

private:
    BasicTestFixture* m_relayTest;
};

//-------------------------------------------------------------------------------------------------


class MediatorConnectorCluster
{
public:
    struct Context
    {
        nx::hpm::MediatorFunctionalTest& mediator;
        nx::hpm::api::MediatorConnector connector;

        Context(
            nx::hpm::MediatorFunctionalTest& mediator,
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

    const nx::hpm::test::MediatorCluster& cluster() const;

private:
    nx::hpm::test::MediatorCluster m_cluster;
    std::vector<std::unique_ptr<Context>> m_mediators;
};

//-------------------------------------------------------------------------------------------------

class BasicTestFixture:
    public nx::utils::test::TestWithTemporaryDirectory
{
    using base_type = nx::utils::test::TestWithTemporaryDirectory;

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
    void SetUp();

    /**
     * Mediator.
     */

    void addMediator();
    int mediatorCount() const;
    void startMediator(int index = 0);
    void restartMediator(int index = 0);
    const nx::hpm::test::MediatorCluster& mediatorCluster() const;
    nx::hpm::MediatorFunctionalTest& mediator(int index = 0);
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
    std::unique_ptr<MediatorConnectorCluster> m_mediatorCluster;
    hpm::api::SystemCredentials m_cloudSystemCredentials;
    std::unique_ptr<nx::network::http::TestHttpServer> m_httpServer;
    nx::network::http::TestHttpServer m_cloudModulesXmlProvider;
    nx::utils::Url m_staticUrl;
    std::list<std::unique_ptr<nx::network::http::AsyncClient>> m_httpClients;
    std::atomic<int> m_unfinishedRequestsLeft;
    std::optional<std::string> m_remotePeerName;
    MediatorApiProtocol m_mediatorApiProtocol = MediatorApiProtocol::http;
    int m_initFlags = 0;
    std::vector<StartupArgument> m_relayStartupArguments;

    nx::utils::SyncQueue<HttpRequestResult> m_httpRequestResults;
    nx::network::http::BufferType m_expectedMsgBody;
    std::unique_ptr<network::AbstractStreamSocket> m_clientSocket;

    std::unique_ptr<ProxyContext> m_proxyBeforeMediator;

    std::unique_ptr<nx::cloud::relay::test::TrafficRelayCluster> m_relays;
    int m_relayCount;
    std::optional<std::chrono::seconds> m_disconnectedPeerTimeout;

    void initializeCloudModulesXmlWithDirectStunPort();
    void initializeCloudModulesXmlWithStunOverHttp();
    void startHttpServer(nx::hpm::api::MediatorConnector* connector);
    void onHttpRequestDone(
        std::list<std::unique_ptr<nx::network::http::AsyncClient>>::iterator clientIter);
    void waitForServerStatusOnRelay(ServerRelayStatus status);

    virtual void peerAdded(const std::string& /*serverName*/) {}
    virtual void peerRemoved(const std::string& /*serverName*/) {}
    void setUpRemoteRelayPeerPoolFactoryFunc();
    void setUpPublicIpFactoryFunc();
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
