#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/ssl/ssl_stream_socket.h>
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
    public BasicComponentTest
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
        for (auto& httpClient: m_httpClients)
            httpClient->pleaseStopSync();
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
        auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
        httpClient->doGet(nx::network::url::Builder(m_baseUrl)
            .appendPath(lm("/%1/%2").arg(m_peerName).arg(kTestRequestPath)));
        m_httpClients.push_back(std::move(httpClient));
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
            std::make_unique<nx::network::ssl::ServerSideStreamSocket>(
                std::move(m_prevAcceptedConnection)));
        m_serverConnection->setOnConnectionClosed(
            [this](auto... args) { onConnectionClosed(args...); });
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
    std::vector<std::unique_ptr<nx::network::http::AsyncClient>> m_httpClients;
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

    void onConnectionClosed(SystemError::ErrorCode closeReason)
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
