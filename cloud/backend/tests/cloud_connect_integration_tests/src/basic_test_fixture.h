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
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relay/model/abstract_remote_relay_peer_pool.h>
#include <nx/cloud/relay/test_support/traffic_relay_launcher.h>
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>

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

using Relay = nx::cloud::relay::test::Launcher;
using RelayPtr = std::unique_ptr<Relay>;

using RelayPtrList = std::vector<RelayPtr>;

class BasicTestFixture;

class MemoryRemoteRelayPeerPool: public nx::cloud::relay::model::AbstractRemoteRelayPeerPool
{
public:
    MemoryRemoteRelayPeerPool(BasicTestFixture* relayTest) :
        m_relayTest(relayTest)
    {}

    virtual bool connectToDb() override;
    virtual bool isConnected() const override;

    virtual cf::future<std::string> findRelayByDomain(
        const std::string& /*domainName*/) const override;

    virtual cf::future<bool> addPeer( const std::string& domainName) override;
    virtual cf::future<bool> removePeer(const std::string& domainName) override;
    virtual void setNodeId(const std::string& /*nodeId*/) override {}

private:
    BasicTestFixture* m_relayTest;
};

class BasicTestFixture
{
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
    network::SocketAddress relayInstanceEndpoint(RelayPtrList::size_type index) const;

    void addRelayStartupArgument(
        const std::string& name,
        const std::string& value);

protected:
    void SetUp();

    /**
     * Mediator.
     */

    void startMediator();
    void restartMediator();
    nx::hpm::MediatorFunctionalTest& mediator();
    void setMediatorApiProtocol(MediatorApiProtocol mediatorApiProtocol);
    void configureProxyBeforeMediator();
    void blockDownTrafficThroughExistingConnectionsToMediator();

    void startCloudModulesXmlProvider();

    /**
     * Relay.
     */

    void setRelayCount(int count);
    void startRelays();
    void startRelay(int index);
    nx::cloud::relay::test::Launcher& trafficRelay();
    nx::utils::Url relayUrl(int relayNum = 0) const;

    /**
     * Listening server.
     */

    void startServer();
    void stopServer();
    std::string serverSocketCloudAddress() const;
    const hpm::api::SystemCredentials& cloudSystemCredentials() const;
    void setRemotePeerName(const std::string& peerName);

    /**
     * Validation functions.
     */

    void assertCloudConnectionCanBeEstablished();
    void waitUntilCloudConnectionCanBeEstablished();
    SystemError::ErrorCode tryEstablishCloudConnection();

    void startExchangingFixedData();
    void assertDataHasBeenExchangedCorrectly();

    void waitUntilServerIsRegisteredOnMediator();
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
    nx::hpm::MediatorFunctionalTest m_mediator;
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

    RelayPtrList m_relays;
    int m_relayCount;
    std::optional<std::chrono::seconds> m_disconnectedPeerTimeout;

    void initializeCloudModulesXmlWithDirectStunPort();
    void initializeCloudModulesXmlWithStunOverHttp();
    void startHttpServer();
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
