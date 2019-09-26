#include "discovery_server.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/random.h>

#include <nx/cloud/discovery/request_paths.h>

namespace nx::cloud::discovery::test {

namespace {

std::string getClusterId(const nx::network::http::RequestContext& requestContext)
{
    static constexpr char kClusterId[] = "clusterId";
    return requestContext.requestPathParams.getByName(kClusterId);
}

} // namespace

std::string generateInfoJson(const std::string& nodeId)
{
    std::string random = nx::utils::random::generateName(16).toStdString();
    return std::string("{")
        + "\"nodeId\": \"" + nodeId + "\","
        "\"random\": \"" + random + "\""
        "}";
}

//-------------------------------------------------------------------------------------------------
// DiscoveryServer

DiscoveryServer::DiscoveryServer(const std::chrono::milliseconds& nodeLifetime):
    m_nodeLifetime(nodeLifetime)
{
    registerRequestHandlers(&m_httpServer.httpMessageDispatcher());
}

bool DiscoveryServer::bindAndListen()
{
    return m_httpServer.bindAndListen();
}

nx::utils::Url DiscoveryServer::url() const
{
    return nx::network::url::Builder().
        setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_httpServer.serverAddress()).toUrl();
}

int DiscoveryServer::onlineNodesCount() const
{
    QnMutexLocker lock(&m_mutex);
    return (int)m_onlineNodes.size();
}

void DiscoveryServer::mockupClientPublicIpAddress(
    const std::string& nodeId,
    const std::string& publicIpAddress)
{
    QnMutexLocker lock(&m_mutex);
    m_clientPublicIpAddresses[nodeId] = publicIpAddress;
}

void DiscoveryServer::subscribeToNodeDiscovered(
    nx::utils::MoveOnlyFunc<void(const std::string& , const NodeInfo&)> handler,
    nx::utils::SubscriptionId* const outId)
{
    m_nodeDiscoveredSubscription.subscribe(std::move(handler), outId);
}

void DiscoveryServer::unsubscribeFromNodeDiscovered(const nx::utils::SubscriptionId& subscriptionId)
{
    m_nodeDiscoveredSubscription.removeSubscription(subscriptionId);
}

void DiscoveryServer::registerRequestHandlers(
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::post,
        kRegisterNodePath,
        [this](auto&&... args) { serveRegisterNode(std::forward<decltype(args)>(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        kRegisterNodePath,
        [this](auto&&... args) { serveGetOnlineNodes(std::forward<decltype(args)>(args)...); });
}

void DiscoveryServer::serveRegisterNode(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    bool ok = false;
    NodeInfo nodeInfo = QJson::deserialized(requestContext.request.messageBody, NodeInfo(), &ok);
    if (!ok)
        return completionHandler(nx::network::http::StatusCode::badRequest);

    if(getClusterId(requestContext).empty())
        return completionHandler(nx::network::http::StatusCode::badRequest);

    if (!haveNodeId(getClusterId(requestContext), nodeInfo.nodeId))
        emitNodeDiscovered(getClusterId(requestContext), nodeInfo);

    Node node = updateNode(std::move(requestContext), nodeInfo);

    nx::network::http::RequestResult result(nx::network::http::StatusCode::created);
    result.dataSource =
        std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            NodeSerialization::serialized(node));

    completionHandler(std::move(result));
}

void DiscoveryServer::serveGetOnlineNodes(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (!haveClusterId(getClusterId(requestContext)))
        return completionHandler(nx::network::http::StatusCode::badRequest);

    auto onlineNodes = updateOnlineNodes(getClusterId(requestContext));

    nx::network::http::RequestResult result(nx::network::http::StatusCode::ok);
    result.dataSource =
        std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            NodeSerialization::serialized(onlineNodes));

    completionHandler(std::move(result));
}

bool DiscoveryServer::haveClusterId(const std::string& clusterId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_onlineNodes.find(clusterId) != m_onlineNodes.end();
}

bool DiscoveryServer::haveNodeId(const std::string& clusterId, const std::string& nodeId) const
{
    QnMutexLocker lock(&m_mutex);
    auto nodesByCluster = m_onlineNodes.find(clusterId);
    if (nodesByCluster == m_onlineNodes.end())
        return false;
    return nodesByCluster->second.find(nodeId) != nodesByCluster->second.end();
}

Node DiscoveryServer::updateNode(
    nx::network::http::RequestContext requestContext,
    const NodeInfo& nodeInfo)
{
    QnMutexLocker lock(&m_mutex);
    Node& node = m_onlineNodes[getClusterId(requestContext)][nodeInfo.nodeId];

    if (node.nodeId.empty())
        node.nodeId = nodeInfo.nodeId;

    node.urls = nodeInfo.urls;

    auto it = m_clientPublicIpAddresses.find(node.nodeId);
    if (it != m_clientPublicIpAddresses.end())
        node.publicIpAddress = it->second;

    node.infoJson = nodeInfo.infoJson;
    node.expirationTime = std::chrono::system_clock::now() + m_nodeLifetime;

    return node;
}

std::vector<Node> DiscoveryServer::updateOnlineNodes(const std::string& clusterId)
{
    QnMutexLocker lock(&m_mutex);
    std::vector<Node> onlineNodes;
    auto& nodes = m_onlineNodes[clusterId];
    for (auto it = nodes.begin(); it != nodes.end();)
    {
        if (it->second.expirationTime <= std::chrono::system_clock::now())
        {
            it = nodes.erase(it);
        }
        else
        {
            onlineNodes.emplace_back(it->second);
            ++it;
        }
    }
    return onlineNodes;
}

void DiscoveryServer::emitNodeDiscovered(const std::string& clusterId, const NodeInfo& nodeInfo)
{
    m_nodeDiscoveredSubscription.notify(clusterId, nodeInfo);
}

} // namespace nx::cloud::discovery::test
