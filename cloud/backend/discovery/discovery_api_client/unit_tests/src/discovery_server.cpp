#include "discovery_server.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/buffer_source.h>

#include <nx/cloud/discovery/request_paths.h>

namespace nx::cloud::discovery::test {

namespace {

static constexpr char kClusterId[] = "clusterId";

} // namespace

DiscoveryServer::DiscoveryServer(const std::string& clusterId):
    m_clusterId(clusterId)
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
        .setEndpoint(m_httpServer.serverAddress());
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
    if (!requestContainsThisClusterId(requestContext))
        return completionHandler(nx::network::http::StatusCode::badRequest);

    bool ok = false;
    NodeInfo nodeInfo = QJson::deserialized(requestContext.request.messageBody, NodeInfo(), &ok);
    if (!ok)
        return completionHandler(nx::network::http::StatusCode::badRequest);

    Node node = updateNode(nodeInfo, requestContext.connection->clientEndpoint());

    nx::network::http::RequestResult result(nx::network::http::StatusCode::created);
    result.dataSource =
        std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            QJson::serialized(node));

    completionHandler(std::move(result));
}

void DiscoveryServer::serveGetOnlineNodes(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (!requestContainsThisClusterId(requestContext))
        return completionHandler(nx::network::http::StatusCode::badRequest);

    auto onlineNodes = getOnlineNodes();

    nx::network::http::RequestResult result(nx::network::http::StatusCode::ok);
    result.dataSource =
        std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            QJson::serialized(onlineNodes));

    completionHandler(std::move(result));
}

bool DiscoveryServer::requestContainsThisClusterId(
    const nx::network::http::RequestContext& requestContext) const
{
    return requestContext.requestPathParams.getByName(kClusterId) == m_clusterId;
}

Node DiscoveryServer::updateNode(
    const NodeInfo& nodeInfo,
    const nx::network::SocketAddress& nodeEndpoint)
{
    QnMutexLocker lock(&m_mutex);
    Node& node = m_onlineNodes[nodeInfo.nodeId];

    if (node.nodeId.empty())
        node.nodeId = nodeInfo.nodeId;

    if (node.host.empty())
        node.host = nodeEndpoint.toStdString();

    node.infoJson = nodeInfo.infoJson;
    node.expirationTime = std::chrono::system_clock::now() + std::chrono::seconds(3);

    return node;
}

std::vector<Node> DiscoveryServer::getOnlineNodes()
{
    QnMutexLocker lock(&m_mutex);
    std::vector<Node> onlineNodes;
    for (auto it = m_onlineNodes.begin(); it != m_onlineNodes.end();)
    {
        if (it->second.expirationTime <= std::chrono::system_clock::now())
        {
            it = m_onlineNodes.erase(it);
        }
        else
        {
            onlineNodes.emplace_back(it->second);
            ++it;
        }

    }
    return onlineNodes;
}

} // namespace nx::cloud::discovery::test
