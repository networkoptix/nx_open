#include <gtest/gtest.h>

#include "basic_relay_api_client_test_fixture.h"
#include "relay_api_client_acceptance_tests.h"

namespace nx::cloud::relay::api::test {

class RelayApiClientOverHttpUpgradeTypeSet:
    public BasicRelayApiClientTestFixture
{
    using base_type = BasicRelayApiClientTestFixture;

public:
    using Client = ClientOverHttpUpgrade;

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
                lm("%1/%2").args(baseUrlPath, kServerIncomingConnectionsPath).toQString()),
            std::bind(&RelayApiClientOverHttpUpgradeTypeSet::beginListeningHandler, this,
                _1, _2));
    }

private:
    void beginListeningHandler(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        serializeToHeaders(&requestContext.response->headers, beginListeningResponse());

        network::http::RequestResult requestResult(
            nx::network::http::StatusCode::switchingProtocols);
        requestResult.connectionEvents.onResponseHasBeenSent =
            [this](network::http::HttpServerConnection* connection)
            {
                saveTunnelConnection(connection->takeSocket());
            };
        completionHandler(std::move(requestResult));
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    RelayApiClientOverHttpUpgrade,
    RelayApiClientAcceptance,
    RelayApiClientOverHttpUpgradeTypeSet);

} // namespace nx::cloud::relay::api::test
