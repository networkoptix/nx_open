#include "discovery_server.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/buffer_source.h>

#include <nx/cloud/discovery/request_paths.h>

namespace nx::cloud::discovery::test {

using namespace nx::network::http;
using namespace std::chrono;

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
        setScheme(kUrlSchemeName)
        .setEndpoint(m_httpServer.serverAddress());
}

void DiscoveryServer::registerRequestHandlers(
    server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        Method::post,
        kRegisterNodePath,
        [this](auto&&... args) { serveRegisterNode(std::move(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        Method::get,
        kRegisterNodePath,
        [this](auto&&... args) { serveGetOnlineNodes(std::move(args)...); });
}

void DiscoveryServer::serveRegisterNode(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (!requestContainsThisClusterId(requestContext))
        return completionHandler(StatusCode::badRequest);

    bool ok = false;
    NodeInfo nodeInfo = QJson::deserialized(requestContext.request.messageBody, NodeInfo(), &ok);
    if (!ok)
        return completionHandler(StatusCode::badRequest);

    Node node = updateNode(nodeInfo, requestContext.connection->clientEndpoint());

    RequestResult result(StatusCode::created);
    result.dataSource =
        std::make_unique<BufferSource>("application/json", QJson::serialized(node));

    completionHandler(std::move(result));
}

void DiscoveryServer::serveGetOnlineNodes(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (!requestContainsThisClusterId(requestContext))
        return completionHandler(StatusCode::badRequest);

    auto onlineNodes = getOnlineNodes();

    RequestResult result(StatusCode::ok);
    result.dataSource =
        std::make_unique<BufferSource>("application/json", QJson::serialized(onlineNodes));

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
    node.expirationTime = system_clock::now() + seconds(3);

    return node;
}

std::vector<Node> DiscoveryServer::getOnlineNodes()
{
    QnMutexLocker lock(&m_mutex);
    std::vector<Node> onlineNodes;
    for (auto it = m_onlineNodes.begin(); it != m_onlineNodes.end();)
    {
        if (it->second.expirationTime <= system_clock::now())
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
