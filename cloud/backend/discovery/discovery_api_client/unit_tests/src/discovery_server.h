#pragma once

#include <map>

#include <nx/network/http/test_http_server.h>

#include <nx/cloud/discovery/node.h>

#include <nx/network/aio/timer.h>

namespace nx::cloud::discovery::test {

class DiscoveryServer:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    DiscoveryServer(const std::string& clusterId);
    ~DiscoveryServer();

    bool bindAndListen();

    nx::utils::Url url() const;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

private:
    struct NodeContext
    {
        Node node;
        nx::network::aio::Timer timer;
        bool isInitialized = false;
    };

    virtual void stopWhileInAioThread() override;

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

    void initializeNodeContext(
        const std::string& nodeId,
        const nx::network::SocketAddress& clientEndpoint,
        NodeContext* nodeContext);

    Node updateNode(
        const NodeInfo& nodeInfo,
        const nx::network::SocketAddress& nodeEndpoint);

    std::vector<Node> getOnlineNodes() const;

    void startTimer(NodeContext& nodeCOntext);

private:
    const std::string m_clusterId;

    mutable QnMutex m_mutex;
    std::map<std::string /*nodeId*/, NodeContext> m_onlineNodes;

    nx::network::http::TestHttpServer m_httpServer;
};

} // namespace nx::cloud::discovery::test
