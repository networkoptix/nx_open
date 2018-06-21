#pragma once

#include <string>

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx::cloud::relay::api::test {

class BasicRelayApiClientTestFixture
{
public:
    void registerClientSessionHandler(
        nx::network::http::TestHttpServer* httpServer,
        const std::string& baseUrlPath,
        const std::string& listeningPeerName)
    {
        const auto realPath = nx::network::http::rest::substituteParameters(
            kServerClientSessionsPath,
            { listeningPeerName.c_str() });

        const auto handlerPath = network::url::normalizePath(lm("%1%2")
            .args(baseUrlPath, realPath).toQString());

        httpServer->registerStaticProcessor(
            handlerPath,
            "{\"result\": \"ok\"}",
            "application/json");
    }

    void setBeginListeningResponse(const BeginListeningResponse& response)
    {
        m_beginListeningResponse = response;
    }

    const BeginListeningResponse& beginListeningResponse() const
    {
        return m_beginListeningResponse;
    }

private:
    BeginListeningResponse m_beginListeningResponse;
};

} // namespace nx::cloud::relay::api::test
