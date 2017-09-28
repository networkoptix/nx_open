#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stun/async_client.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relay/model/abstract_remote_relay_peer_pool.h>
#include <nx/cloud/relay/test_support/traffic_relay_launcher.h>
#include <libconnection_mediator/src/test_support/mediator_functional_test.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class PredefinedCredentialsProvider:
    public hpm::api::AbstractCloudSystemCredentialsProvider
{
public:
    void setCredentials(hpm::api::SystemCredentials cloudSystemCredentials);

    virtual boost::optional<hpm::api::SystemCredentials> getSystemCredentials() const override;

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

    virtual cf::future<std::string> findRelayByDomain(
        const std::string& /*domainName*/) const override;

    virtual cf::future<bool> addPeer(
        const std::string& domainName,
        const std::string& relayHost) override;

    virtual cf::future<bool> removePeer(const std::string& domainName) override;

private:
    BasicTestFixture* m_relayTest;
};

class BasicTestFixture:
    public ::testing::Test
{
public:
    enum Flag
    {
        doNotInitializeMediatorConnection = 1,
    };

    BasicTestFixture(
        int relayCount = 1,
        boost::optional<std::chrono::seconds> disconnectedPeerTimeout = boost::none);
    ~BasicTestFixture();

    /**
     * @param flags Bitset of BasicTestFixture::Flag values.
     */
    void setInitFlags(int flags);
    SocketAddress relayInstanceEndpoint(RelayPtrList::size_type index) const;

protected:
    virtual void SetUp() override;

    void startServer();
    void stopServer();
    QUrl relayUrl(int relayNum = 0) const;
    void restartMediator();

    void assertConnectionCanBeEstablished();

    void startExchangingFixedData();
    void assertDataHasBeenExchangedCorrectly();

    nx::hpm::MediatorFunctionalTest& mediator();
    nx::cloud::relay::test::Launcher& trafficRelay();
    nx::String serverSocketCloudAddress() const;
    const hpm::api::SystemCredentials& cloudSystemCredentials() const;

    void waitUntilServerIsRegisteredOnMediator();
    void waitUntilServerIsRegisteredOnTrafficRelay();
    void waitUntilServerIsUnRegisteredOnTrafficRelay();

    void setRemotePeerName(const nx::String& peerName);
    void setMediatorApiProtocol(MediatorApiProtocol mediatorApiProtocol);

    const std::unique_ptr<AbstractStreamSocket>& clientSocket();

private:
    friend class MemoryRemoteRelayPeerPool;

    struct HttpRequestResult
    {
        nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined;
        nx_http::BufferType msgBody;
    };

    enum class ServerRelayStatus
    {
        registered,
        unregistered
    };

    QnMutex m_mutex;
    const nx_http::BufferType m_staticMsgBody;
    nx::hpm::MediatorFunctionalTest m_mediator;
    hpm::api::SystemCredentials m_cloudSystemCredentials;
    std::unique_ptr<TestHttpServer> m_httpServer;
    TestHttpServer m_cloudModulesXmlProvider;
    QUrl m_staticUrl;
    std::list<std::unique_ptr<nx_http::AsyncClient>> m_httpClients;
    std::atomic<int> m_unfinishedRequestsLeft;
    boost::optional<nx::String> m_remotePeerName;
    MediatorApiProtocol m_mediatorApiProtocol = MediatorApiProtocol::http;
    int m_initFlags = 0;

    nx::utils::SyncQueue<HttpRequestResult> m_httpRequestResults;
    nx_http::BufferType m_expectedMsgBody;
    std::unique_ptr<AbstractStreamSocket> m_clientSocket;

    RelayPtrList m_relays;
    int m_relayCount;
    boost::optional<std::chrono::seconds> m_disconnectedPeerTimeout;


    void initializeCloudModulesXmlWithDirectStunPort();
    void initializeCloudModulesXmlWithStunOverHttp();
    void startHttpServer();
    void onHttpRequestDone(
        std::list<std::unique_ptr<nx_http::AsyncClient>>::iterator clientIter);
    void startRelays();
    void startRelay(int index);
    void waitForServerStatusOnRelay(ServerRelayStatus status);

    virtual void peerAdded(const std::string& /*serverName*/, const std::string& /*relay*/) {}
    virtual void peerRemoved(const std::string& /*serverName*/) {}
    void setUpRemoteRelayPeerPoolFactoryFunc();
    void setUpPublicIpFactoryFunc();
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
