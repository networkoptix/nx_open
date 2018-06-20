#include <gtest/gtest.h>

#include "relay_api_client_acceptance_tests.h"

namespace nx {
namespace cloud {
namespace relay {
namespace api {
namespace test {

struct RelayApiClientOverHttpUpgradeTypeSet
{
    using Client = ClientOverHttpUpgrade;

    void initializeHttpServer(
        nx::network::http::TestHttpServer* httpServer,
        const std::string& baseUrlPath,
        const std::string& listeningPeerName)
    {
        using namespace std::placeholders;

        const auto realPath = nx::network::http::rest::substituteParameters(
            kServerClientSessionsPath,
            { listeningPeerName.c_str() });

        const auto handlerPath = network::url::normalizePath(lm("%1%2")
            .args(baseUrlPath, realPath).toQString());

        httpServer->registerStaticProcessor(
            handlerPath,
            "{\"result\": \"ok\"}",
            "application/json");

        httpServer->registerRequestProcessorFunc(
            network::url::normalizePath(
                lm("%1/%2").args(baseUrlPath, kServerIncomingConnectionsPath).toQString()),
            std::bind(&RelayApiClientOverHttpUpgradeTypeSet::beginListeningHandler, this,
                _1, _2, _3, _4, _5));
    }

    void beginListeningHandler(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_expectedBeginListeningResponse.preemptiveConnectionCount =
            nx::utils::random::number<int>(1, 99);
        if (nx::utils::random::number<int>(0, 1) > 0)
        {
            m_expectedBeginListeningResponse.keepAliveOptions = nx::network::KeepAliveOptions();
            m_expectedBeginListeningResponse.keepAliveOptions->probeCount =
                nx::utils::random::number<int>(1, 99);
            m_expectedBeginListeningResponse.keepAliveOptions->inactivityPeriodBeforeFirstProbe =
                std::chrono::seconds(nx::utils::random::number<int>(1, 99));
            m_expectedBeginListeningResponse.keepAliveOptions->probeSendPeriod =
                std::chrono::seconds(nx::utils::random::number<int>(1, 99));
        }

        serializeToHeaders(&response->headers, m_expectedBeginListeningResponse);

        completionHandler(nx::network::http::StatusCode::switchingProtocols);
    }

    const BeginListeningResponse& expectedBeginListeningResponse() const
    {
        return m_expectedBeginListeningResponse;
    }

private:
    BeginListeningResponse m_expectedBeginListeningResponse;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    RelayApiClientOverHttpUpgrade,
    RelayApiClientAcceptance,
    RelayApiClientOverHttpUpgradeTypeSet);

} // namespace test
} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
