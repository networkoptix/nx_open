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
    bindToAioThread(getAioThread());
    registerRequestHandlers(&m_httpServer.httpMessageDispatcher());
}

DiscoveryServer::~DiscoveryServer()
{
    pleaseStopSync();
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

void DiscoveryServer::bindToAioThread(nx::network::aio::AbstractAioThread * aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_httpServer.bindToAioThread(aioThread);

    {
        QnMutexLocker lock(&m_mutex);
        for (auto& element : m_onlineNodes)
            element.second.timer.bindToAioThread(aioThread);
    }
}

void DiscoveryServer::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    QnMutexLocker lock(&m_mutex);
    for (auto& element : m_onlineNodes)
        element.second.timer.pleaseStopSync();
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
    std::shared_ptr<nx::network::aio::Timer> timer;

    QnMutexLocker lock(&m_mutex);
    NodeContext& nodeContext = m_onlineNodes[nodeInfo.nodeId];
    if (!nodeContext.isInitialized)
        initializeNodeContext(nodeInfo.nodeId, nodeEndpoint, &nodeContext);

    ++nodeContext.updates;
    nodeContext.node.infoJson = nodeInfo.infoJson;
    nodeContext.node.expirationTime = system_clock::now() + seconds(3);

    startTimer(nodeContext);

    return nodeContext.node;
}

std::vector<Node> DiscoveryServer::getOnlineNodes() const
{
    QnMutexLocker lock(&m_mutex);
    std::vector<Node> onlineNodes;
    std::transform(m_onlineNodes.begin(), m_onlineNodes.end(),
        std::back_inserter(onlineNodes), [](const auto& element) { return element.second.node; });
    return onlineNodes;
}

void DiscoveryServer::initializeNodeContext(
    const std::string& nodeId,
    const nx::network::SocketAddress& clientEndpoint,
    NodeContext* outNodeContext)
{
    outNodeContext->initializedAt = system_clock::now();
    qDebug() << nodeId.c_str() << ": first init at:"
        << duration_cast<milliseconds>(outNodeContext->initializedAt.time_since_epoch()).count();
    outNodeContext->node.nodeId = nodeId;
    outNodeContext->node.host = clientEndpoint.toStdString();
    outNodeContext->timer.bindToAioThread(getAioThread());
    outNodeContext->isInitialized = true;
}

void DiscoveryServer::startTimer(NodeContext& nodeContext)
{
    nodeContext.timer.start(
        std::chrono::milliseconds(100),
        [this, &nodeContext]()
        {
            QnMutexLocker lock(&m_mutex);
            if (nodeContext.node.expirationTime <= system_clock::now())
            {
                auto nowMsec = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
                auto initAtMsec =
                    duration_cast<milliseconds>(nodeContext.initializedAt.time_since_epoch());

                qDebug() << nodeContext.node.nodeId.c_str()
                    << "expired at:" << nowMsec.count()
                    << ", lifetime:" << (nowMsec - initAtMsec).count() << "milliseconds."
                    << "number of updates:" << nodeContext.updates;

                nodeContext.timer.pleaseStopSync();
                m_onlineNodes.erase(nodeContext.node.nodeId);
            }
            else
            {
                startTimer(nodeContext);
            }
        });
};


} // namespace nx::cloud::discovery::test
