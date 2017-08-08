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

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <libtraffic_relay/src/test_support/traffic_relay_launcher.h>

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

struct RelayContext
{
    RelayPtr relay;
    int port;

    RelayContext(RelayPtr relay, int port):
        relay(std::move(relay)),
        port(port)
    {}
};

using RelayContextList = std::vector<RelayContext>;

class BasicTestFixture:
    public ::testing::Test
{
public:
    BasicTestFixture(int relayCount = 1);
    ~BasicTestFixture();

protected:
    virtual void SetUp() override;

    void startServer();
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

    void setRemotePeerName(const nx::String& peerName);
    void setMediatorApiProtocol(MediatorApiProtocol mediatorApiProtocol);

    const std::unique_ptr<AbstractStreamSocket>& clientSocket();

private:
    struct HttpRequestResult
    {
        nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined;
        nx_http::BufferType msgBody;
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

    nx::utils::SyncQueue<HttpRequestResult> m_httpRequestResults;
    nx_http::BufferType m_expectedMsgBody;
    std::unique_ptr<AbstractStreamSocket> m_clientSocket;

    RelayContextList m_relays;
    int m_relayCount;


    void initializeCloudModulesXmlWithDirectStunPort();
    void initializeCloudModulesXmlWithStunOverHttp();
    void startHttpServer();
    void onHttpRequestDone(
        std::list<std::unique_ptr<nx_http::AsyncClient>>::iterator clientIter);
    void startRelays();
    void startRelay(int port);
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
