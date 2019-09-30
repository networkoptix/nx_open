#pragma once

#include <map>

#include <nx/network/http/test_http_server.h>

#include <nx/cloud/discovery/node.h>

#include <nx/network/aio/timer.h>

#include <nx/utils/subscription.h>

namespace nx::cloud::discovery::test {

NX_DISCOVERY_CLIENT_API std::string generateInfoJson(const std::string& nodeId);

class NX_DISCOVERY_CLIENT_API DiscoveryServer
{
public:
    DiscoveryServer(const std::chrono::milliseconds& nodeLifetime = std::chrono::seconds(1));

    bool bindAndListen();

    nx::utils::Url url() const;

    int onlineNodesCount() const;

    void mockupClientPublicIpAddress(
        const std::string& nodeId,
        const std::string& publicIpAddress);

    /**
     * Event is emitted before the node is actually registered with discovery service.
     */
    void subscribeToNodeDiscovered(
        nx::utils::MoveOnlyFunc<void(const std::string&, const NodeInfo&)> handler,
        nx::utils::SubscriptionId* const outId);

    void unsubscribeFromNodeDiscovered(const nx::utils::SubscriptionId& subscriptionId);

private:
    void registerRequestHandlers(
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher);

    void serveRegisterNode(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void serveGetOnlineNodes(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    bool haveClusterId(const std::string& clusterId) const;
    bool haveNodeId(const std::string& clusterId, const std::string& nodeId) const;

    Node updateNode(nx::network::http::RequestContext requestContext, const NodeInfo& nodeInfo);

    std::vector<Node> updateOnlineNodes(const std::string& clusterId);

    /**
     * Subscribe to a node discover event. Event is emitted before node is actually registered
     * with discovery service.
     */
    void emitNodeDiscovered(const std::string& clusteId, const NodeInfo& nodeInfo);

private:
    const std::chrono::milliseconds m_nodeLifetime;

    mutable QnMutex m_mutex;
    std::map<std::string /*clusterId*/, std::map<std::string /*nodeId*/, Node>> m_onlineNodes;
    std::map<std::string/*nodeId*/, std::string/*publcIpAddress*/> m_clientPublicIpAddresses;

    nx::utils::Subscription<std::string/*clusterId*/, NodeInfo> m_nodeDiscoveredSubscription;

    nx::network::http::TestHttpServer m_httpServer;
};

} // namespace nx::cloud::discovery::test
