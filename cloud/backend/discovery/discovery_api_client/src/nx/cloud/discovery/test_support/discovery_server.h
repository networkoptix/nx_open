#pragma once

#include <map>

#include <nx/network/http/test_http_server.h>

#include <nx/cloud/discovery/node.h>

#include <nx/network/aio/timer.h>

namespace nx::cloud::discovery::test {

NX_DISCOVERY_CLIENT_API std::string generateInfoJson(const std::string& nodeId);

class NX_DISCOVERY_CLIENT_API DiscoveryServer
{
public:
    DiscoveryServer(
        const std::string& clusterId,
        const std::chrono::milliseconds& nodeLifetime = std::chrono::seconds(1));

    bool bindAndListen();

    nx::utils::Url url() const;

private:
    void registerRequestHandlers(
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher);

    void serveRegisterNode(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void serveGetOnlineNodes(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    bool requestContainsThisClusterId(
        const nx::network::http::RequestContext& requestContext) const;

    Node updateNode(const NodeInfo& nodeInfo);

    std::vector<Node> updateOnlineNodes();

private:
    const std::string m_clusterId;
    const std::chrono::milliseconds m_nodeLifetime;

    mutable QnMutex m_mutex;
    std::map<std::string/*nodeId*/, Node> m_onlineNodes;

    nx::network::http::TestHttpServer m_httpServer;
};

} // namespace nx::cloud::discovery::test
