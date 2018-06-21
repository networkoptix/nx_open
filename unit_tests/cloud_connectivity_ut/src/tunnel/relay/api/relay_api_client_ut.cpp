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
        const std::string& baseUrlPath,
        const std::string& listeningPeerName)
    {
        using namespace std::placeholders;

        registerClientSessionHandler(
            httpServer,
            baseUrlPath,
            listeningPeerName);

        httpServer->registerRequestProcessorFunc(
            network::url::normalizePath(
                lm("%1/%2").args(baseUrlPath, kServerTunnelPath).toQString()),
            std::bind(&RelayApiClientOverHttpUpgradeTypeSet::beginListeningHandler, this,
                _1, _2, _3, _4, _5));
    }

private:
    void beginListeningHandler(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        serializeToHeaders(&response->headers, beginListeningResponse());

        completionHandler(nx::network::http::StatusCode::switchingProtocols);
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    RelayApiClientOverHttpUpgrade,
    RelayApiClientAcceptance,
    RelayApiClientOverHttpUpgradeTypeSet);

} // namespace nx::cloud::relay::api::test
