#pragma once

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/http/httptypes.h>
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

class BasicTestFixture:
    public ::testing::Test
{
public:
    BasicTestFixture();
    ~BasicTestFixture();

protected:
    virtual void SetUp() override;

    void startServer();
    QUrl relayUrl() const;

    void assertConnectionCanBeEstablished();

    void startExchangingFixedData();
    void assertDataHasBeenExchangedCorrectly();

    nx::hpm::MediatorFunctionalTest& mediator();
    nx::cloud::relay::test::Launcher& trafficRelay();
    nx::String serverSocketCloudAddress() const;

private:
    struct HttpRequestResult
    {
        nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined;
        nx_http::BufferType msgBody;
    };

    const nx_http::BufferType m_staticMsgBody;
    nx::hpm::MediatorFunctionalTest m_mediator;
    nx::hpm::AbstractCloudDataProvider::System m_cloudSystemCredentials;
    nx::cloud::relay::test::Launcher m_trafficRelay;
    PredefinedCredentialsProvider m_credentialsProvider;
    std::shared_ptr<nx::stun::AsyncClient> m_stunClient;
    std::unique_ptr<TestHttpServer> m_httpServer;
    QUrl m_staticUrl;
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    nx::utils::SyncQueue<HttpRequestResult> m_httpRequestResults;
    nx_http::BufferType m_expectedMsgBody;
    QUrl m_relayUrl;

    void startHttpServer();
    void onHttpRequestDone();
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
