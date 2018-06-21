#pragma once

#include <string>

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/thread/sync_queue.h>

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

    std::unique_ptr<network::AbstractStreamSocket> takeLastTunnelConnection()
    {
        return m_tunnelConnections.pop();
    }

    void saveTunnelConnection(
        std::unique_ptr<network::AbstractStreamSocket> connection)
    {
        m_tunnelConnections.push(std::move(connection));
    }

private:
    BeginListeningResponse m_beginListeningResponse;
    nx::utils::SyncQueue<std::unique_ptr<network::AbstractStreamSocket>> m_tunnelConnections;
};

} // namespace nx::cloud::relay::api::test
