#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

#include <http/http_api_path.h>
#include <vms_gateway_process.h>

#include "test_setup.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

static const char* const kTestRequestPath = "ProxyUsingRelayingTest";

class ProxyUsingRelaying:
    public BasicComponentTest,
    public network::server::StreamConnectionHolder<nx::network::http::AsyncMessagePipeline>
{
public:
    ProxyUsingRelaying():
        m_peerName(nx::utils::generateRandomName(7))
    {
    }

    ~ProxyUsingRelaying()
    {
        if (m_relayConnectionAcceptor)
            m_relayConnectionAcceptor->pleaseStopSync();
        if (m_httpClient)
            m_httpClient->pleaseStopSync();
        if (m_serverConnection)
            m_serverConnection->pleaseStopSync();
    }

protected:
    void waitForPeerToEstablishRelayConnections()
    {
        while (!moduleInstance()->impl()->relayEngine()
            .listeningPeerPool().isPeerOnline(m_peerName.toStdString()))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void whenIssueProxyRequest()
    {
        m_httpClient = std::make_unique<nx::network::http::AsyncClient>();
        m_httpClient->doGet(nx::network::url::Builder(m_baseUrl)
            .appendPath(lm("/%1/%2").arg(m_peerName).arg(kTestRequestPath)));
    }

    void thenConnectionIsAccepted()
    {
        m_prevAcceptedConnection = m_acceptedConnections.pop();
        ASSERT_NE(nullptr, m_prevAcceptedConnection.get());
    }

    void andRequestProxiedToTheServerConnection()
    {
        using namespace std::placeholders;

        ASSERT_TRUE(m_prevAcceptedConnection->setRecvTimeout(
            std::chrono::milliseconds::zero()));

        m_serverConnection = std::make_unique<nx::network::http::AsyncMessagePipeline>(
            this,
            std::make_unique<nx::network::deprecated::SslSocket>(
                std::move(m_prevAcceptedConnection),
                true));
        m_serverConnection->setMessageHandler(
            std::bind(&ProxyUsingRelaying::saveMessageReceived, this, _1));
        m_serverConnection->startReadingConnection();

        m_receivedMessageQueue.pop();
    }

private:
    struct BeginListenResult
    {
        relay::api::ResultCode resultCode;
        relay::api::BeginListeningResponse response;
        std::unique_ptr<nx::network::AbstractStreamSocket> connection;
    };

    std::unique_ptr<nx::network::cloud::relay::ConnectionAcceptor> m_relayConnectionAcceptor;
    nx::utils::Url m_baseUrl;
    nx::String m_peerName;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    std::unique_ptr<nx::network::http::AsyncMessagePipeline> m_serverConnection;
    nx::utils::SyncQueue<nx::network::http::Message> m_receivedMessageQueue;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionClosureReasons;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::AbstractStreamSocket>> m_acceptedConnections;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_prevAcceptedConnection;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        ASSERT_TRUE(startAndWaitUntilStarted());

        m_baseUrl = nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName).setEndpoint(endpoint())
            .setPath(nx::cloud::gateway::kApiPathPrefix).toUrl();

        auto relayUrl = m_baseUrl;
        relayUrl.setUserName(m_peerName);

        m_relayConnectionAcceptor =
            std::make_unique<nx::network::cloud::relay::ConnectionAcceptor>(relayUrl);
        m_relayConnectionAcceptor->acceptAsync(
            std::bind(&ProxyUsingRelaying::saveAcceptedConnection, this, _1, _2));
    }

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx::network::http::AsyncMessagePipeline* /*connection*/) override
    {
        m_connectionClosureReasons.push(closeReason);
    }

    void saveMessageReceived(nx::network::http::Message message)
    {
        m_receivedMessageQueue.push(std::move(message));
    }

    void saveAcceptedConnection(
        SystemError::ErrorCode /*systemErrorCode*/,
        std::unique_ptr<nx::network::AbstractStreamSocket> connection)
    {
        m_acceptedConnections.push(std::move(connection));
    }
};

TEST_F(ProxyUsingRelaying, relay_connection_is_used_for_proxying)
{
    waitForPeerToEstablishRelayConnections();

    whenIssueProxyRequest();

    thenConnectionIsAccepted();
    andRequestProxiedToTheServerConnection();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
