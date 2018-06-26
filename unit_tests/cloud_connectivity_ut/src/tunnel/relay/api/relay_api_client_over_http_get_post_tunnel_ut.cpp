#include <nx/network/cloud/tunnel/relay/api/relay_api_client_over_http_get_post_tunnel.h>

#include "basic_relay_api_client_test_fixture.h"
#include "relay_api_client_acceptance_tests.h"

namespace nx::cloud::relay::api::test {

class RelayApiClientOverHttpGetPostTunnelTypeSet:
    public BasicRelayApiClientTestFixture,
    public network::server::StreamConnectionHolder<
        network::http::AsyncMessagePipeline>
{
public:
    using Client = ClientOverHttpGetPostTunnel;

    void initializeHttpServer(
        nx::network::http::TestHttpServer* httpServer,
        const std::string& baseUrlPath)
    {
        using namespace std::placeholders;

        registerClientSessionHandler(
            httpServer,
            baseUrlPath);

        registerOpenClientTunnelViaUpgradeHandler(
            httpServer,
            baseUrlPath);

        httpServer->registerRequestProcessorFunc(
            network::url::normalizePath(
                lm("%1/%2").args(baseUrlPath, kServerTunnelPath).toQString()),
            std::bind(&RelayApiClientOverHttpGetPostTunnelTypeSet::openTunnelChannelDown, this,
                _1, _2, _3, _4, _5),
            nx::network::http::Method::get);
    }

private:
    std::list<std::unique_ptr<network::http::AsyncMessagePipeline>> m_connections;

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        network::http::AsyncMessagePipeline* /*connection*/) override
    {
        // TODO
    }

    void openTunnelChannelDown(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        using namespace std::placeholders;

        serializeToHeaders(&response->headers, beginListeningResponse());

        network::http::RequestResult requestResult(nx::network::http::StatusCode::ok);
        requestResult.connectionEvents.onResponseHasBeenSent =
            std::bind(&RelayApiClientOverHttpGetPostTunnelTypeSet::openUpTunnel, this, _1);

        completionHandler(std::move(requestResult));
    }

    void openUpTunnel(
        nx::network::http::HttpServerConnection* connection)
    {
        using namespace std::placeholders;

        m_connections.push_back(
            std::make_unique<network::http::AsyncMessagePipeline>(
                this,
                connection->takeSocket()));
        m_connections.back()->setMessageHandler(
            std::bind(&RelayApiClientOverHttpGetPostTunnelTypeSet::onMessage, this, _1));
        m_connections.back()->startReadingConnection();
    }

    void onMessage(network::http::Message /*httpMessage*/)
    {
        // TODO
        auto connection = m_connections.back()->takeSocket();
        m_connections.pop_back();
        saveTunnelConnection(std::move(connection));
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    RelayApiClientOverHttpGetPostTunnel,
    RelayApiClientAcceptance,
    RelayApiClientOverHttpGetPostTunnelTypeSet);

} // namespace nx::cloud::relay::api::test
