#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

#include <http/http_api_path.h>

#include "test_setup.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

static const char* const kTestRequestPath = "ProxyUsingRelayingTest";

class ProxyUsingRelaying:
    public BasicComponentTest,
    public network::server::StreamConnectionHolder<nx_http::AsyncMessagePipeline>
{
public:
    ProxyUsingRelaying():
        m_peerName(nx::utils::generateRandomName(7))
    {
    }

    ~ProxyUsingRelaying()
    {
        if (m_relayClient)
            m_relayClient->pleaseStopSync();
        if (m_httpClient)
            m_httpClient->pleaseStopSync();
        if (m_serverConnection)
            m_serverConnection->pleaseStopSync();
    }

protected:
    void givenEstablishedServerConnection()
    {
        using namespace std::placeholders;

        whenEstablishServerConnection();
        thenConnectionIsEstaliblished();

        m_serverConnection = std::make_unique<nx_http::AsyncMessagePipeline>(
            this,
            std::move(m_prevBeginListenResult.connection));
        m_serverConnection->setMessageHandler(
            std::bind(&ProxyUsingRelaying::saveMessageReceived, this, _1));
        m_serverConnection->startReadingConnection();
    }

    void whenIssueProxyRequest()
    {
        m_httpClient = std::make_unique<nx_http::AsyncClient>();
        m_httpClient->doGet(nx::network::url::Builder(m_baseUrl)
            .addPath(lm("/%1/%2").arg(m_peerName).arg(kTestRequestPath)));
    }

    void thenRequestProxiedToTheServerConnection()
    {
        m_receivedMessageQueue.pop();
    }

    void whenEstablishServerConnection()
    {
        using namespace std::placeholders;

        m_relayClient->beginListening(
            m_peerName,
            std::bind(&ProxyUsingRelaying::saveBeginListenResult, this, _1, _2, _3));
    }

    void thenConnectionIsEstaliblished()
    {
        m_prevBeginListenResult = m_beginListenResults.pop();
        ASSERT_EQ(relay::api::ResultCode::ok, m_prevBeginListenResult.resultCode);
    }

private:
    struct BeginListenResult
    {
        relay::api::ResultCode resultCode;
        relay::api::BeginListeningResponse response;
        std::unique_ptr<AbstractStreamSocket> connection;
    };

    std::unique_ptr<nx::cloud::relay::api::Client> m_relayClient;
    QUrl m_baseUrl;
    nx::String m_peerName;
    BeginListenResult m_prevBeginListenResult;
    nx::utils::SyncQueue<BeginListenResult> m_beginListenResults;
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    std::unique_ptr<nx_http::AsyncMessagePipeline> m_serverConnection;
    nx::utils::SyncQueue<nx_http::Message> m_receivedMessageQueue;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_baseUrl = nx::network::url::Builder()
            .setScheme(nx_http::kUrlSchemeName).setEndpoint(endpoint())
            .setPath(nx::cloud::gateway::kApiPathPrefix).toUrl();

        m_relayClient = relay::api::ClientFactory::create(m_baseUrl);
    }

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        nx_http::AsyncMessagePipeline* /*connection*/) override
    {
        // TODO
    }

    void saveBeginListenResult(
        relay::api::ResultCode resultCode,
        relay::api::BeginListeningResponse response,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
        m_beginListenResults.push(
            BeginListenResult{resultCode, std::move(response), std::move(connection)});
    }

    void saveMessageReceived(nx_http::Message message)
    {
        m_receivedMessageQueue.push(std::move(message));
    }
};

TEST_F(ProxyUsingRelaying, server_connection_can_be_established)
{
    whenEstablishServerConnection();
    thenConnectionIsEstaliblished();
}

TEST_F(ProxyUsingRelaying, DISABLED_relay_connection_is_used_for_proxying)
{
    givenEstablishedServerConnection();
    whenIssueProxyRequest();
    thenRequestProxiedToTheServerConnection();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
