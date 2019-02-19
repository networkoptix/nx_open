#include "discovery_api_client.h"

#include <nx/utils/timer_holder.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/buffer_source.h>

#include "request_paths.h"

namespace nx::cloud::discovery {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (NodeInfo)(Node),
    (json),
    _Fields)

//-------------------------------------------------------------------------------------------------
// RequestContext

namespace {

std::set<Node> moveToSet(const std::vector<Node>& nodes)
{
    std::set<Node> nodeSet;
    for (const auto& node : nodes)
        nodeSet.emplace(std::move(node));
    return nodeSet;
}

std::vector<Node> toVector(const std::set<Node>& nodes)
{
    std::vector<Node> nodeList;
    for (const auto& node : nodes)
        nodeList.emplace_back(node);
    return nodeList;
}

} //namespace

DiscoveryClient::RequestContext::RequestContext(
    ResponseReceivedHandler responseReceived)
{
    m_httpClient.setOnSomeMessageBodyAvailable([this]() { updateMessageBody(); });

    m_httpClient.setOnDone(
        [this, responseReceived = std::move(responseReceived)]()
        {
            updateMessageBody();
            responseReceived(m_messageBody);
        });
}

void DiscoveryClient::RequestContext::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    m_httpClient.bindToAioThread(aioThread);
}

void DiscoveryClient::RequestContext::stopWhileInAioThread()
{
    m_timer.cancelSync();
    m_httpClient.pleaseStopSync();
}

void DiscoveryClient::RequestContext::clearMessageBody()
{
    m_messageBody.clear();
}

void DiscoveryClient::RequestContext::updateMessageBody()
{
    m_messageBody += m_httpClient.fetchMessageBodyBuffer();
}

void DiscoveryClient::RequestContext::start(
    nx::network::aio::TimerEventHandler timerEvent)
{
    m_timer.start(std::chrono::milliseconds(1000), std::move(timerEvent));
}

void DiscoveryClient::RequestContext::stop()
{
    m_timer.cancelSync();
}

void DiscoveryClient::RequestContext::doGet(const nx::utils::Url & url)
{
    clearMessageBody();
    m_httpClient.doGet(url);
}

void DiscoveryClient::RequestContext::doPost(
    const nx::utils::Url& url,
    const QByteArray& messageBody)
{
    clearMessageBody();

    m_httpClient.setRequestBody(
        std::make_unique<nx::network::http::BufferSource>("application/json", messageBody));
    m_httpClient.doPost(url);
}

//-------------------------------------------------------------------------------------------------
// DiscoveryClient

DiscoveryClient::DiscoveryClient(
    const nx::utils::Url& discoveryServiceUrl,
    const std::string& clusterId,
    const std::string& nodeId,
    const std::string& infoJson)
    :
    m_discoveryServiceUrl(discoveryServiceUrl)
{
    m_thisNodeInfo.nodeId = nodeId;
    m_thisNodeInfo.infoJson = infoJson;

    setupDiscoveryServiceUrl(clusterId);
}

DiscoveryClient::~DiscoveryClient()
{
    stopWhileInAioThread();
}

void DiscoveryClient::bindToAioThread(nx::network::aio::AbstractAioThread * aioThread)
{
    BaseType::bindToAioThread(aioThread);

    if (m_registerNodeRequest)
        m_registerNodeRequest->bindToAioThread(getAioThread());

    if (m_onlineNodesRequest)
        m_onlineNodesRequest->bindToAioThread(getAioThread());
}

void DiscoveryClient::stopWhileInAioThread()
{
    BaseType::stopWhileInAioThread();
    m_registerNodeRequest->stopWhileInAioThread();
    m_onlineNodesRequest->stopWhileInAioThread();
}

void DiscoveryClient::start()
{
    startRegisterNode();
    startOnlineNodesRequest();
}

void DiscoveryClient::updateInformation(const std::string& infoJson)
{
    QnMutexLocker lock(&m_mutex);
    m_thisNodeInfo.infoJson = infoJson;
    startRegisterNode();
}

std::vector<Node> DiscoveryClient::onlineNodes() const
{
    QnMutexLocker lock(&m_mutex);
    return toVector(m_onlineNodes);
}

void DiscoveryClient::setOnNodeDiscovered(NodeDiscoveredHandler handler)
{
    m_nodeDiscoveredHandler = std::move(handler);
}

void DiscoveryClient::setOnNodeLost(NodeLostHandler handler)
{
    m_nodeLostHandler = std::move(handler);
}

void DiscoveryClient::setupDiscoveryServiceUrl(const std::string& clusterId)
{
    QString nodeRegistryPath(kRegisterNodePath);
    nodeRegistryPath.replace(kClusterIdTemplate, clusterId.c_str());
    m_discoveryServiceUrl.setPath(nodeRegistryPath);
}

void DiscoveryClient::startRegisterNode()
{
    const auto startRegisterNodeRequest =
        [this]()
        {
            m_registerNodeRequest->start(
                [this]()
                {
                    m_registerNodeRequest->doPost(
                        m_discoveryServiceUrl,
                        QJson::serialized(m_thisNodeInfo));
                });
        };

    if (!m_registerNodeRequest)
    {
        m_registerNodeRequest = std::make_unique<RequestContext>(
            [this, startRegisterNodeRequest](QByteArray messageBody)
            {
                m_registerNodeRequest->stop();

                bool ok = false;
                Node node = QJson::deserialized(messageBody, Node(), &ok);
                if (ok)
                {
                    QnMutexLocker lock(&m_mutex);
                    m_thisNode = std::move(node);
                }
                else
                {
                    NX_ERROR(this, lm("Error deserializing register node response"));
                }

                startRegisterNodeRequest();
            });
        m_registerNodeRequest->bindToAioThread(getAioThread());
    }

    startRegisterNodeRequest();
}

void DiscoveryClient::startOnlineNodesRequest()
{
    const auto startOnlineNodesRequest =
        [this]()
        {
            m_onlineNodesRequest->start(
                [this]()
                {
                    m_onlineNodesRequest->doGet(m_discoveryServiceUrl);
                });
        };

    if (!m_onlineNodesRequest)
    {
        m_onlineNodesRequest = std::make_unique<RequestContext>(
            [this, startOnlineNodesRequest](QByteArray messageBody)
            {
                m_onlineNodesRequest->stop();

                bool ok = false;
                std::vector<Node> onlineNodes =
                    QJson::deserialized(messageBody, std::vector<Node>(), &ok);
                if (ok)
                    updateOnlineNodes(onlineNodes);
                else
                    NX_ERROR(this, lm("Error deserializing online nodes response"));

                startOnlineNodesRequest();
            });
        m_onlineNodesRequest->bindToAioThread(getAioThread());
    }

    startOnlineNodesRequest();
}

void DiscoveryClient::updateOnlineNodes(const std::vector<Node>& onlineNodes)
{
    auto onlineNodeSet = moveToSet(onlineNodes);

    // Nodes that are not present in onlineNodeSet and not m_onlineNodes are newly online.
    for (const auto& node : onlineNodeSet)
    {
        if (m_onlineNodes.find(node) == m_onlineNodes.end())
            emitNodeDiscovered(node);
    }

    // Nodes that are present in m_onlineNodes and not in onlineNodeSet have been lost.
    for (const auto& node : m_onlineNodes)
    {
        if (onlineNodeSet.find(node) == onlineNodeSet.end())
            emitNodeLost(node.nodeId);
    }

    {
        QnMutexLocker lock(&m_mutex);
        m_onlineNodes = std::move(onlineNodeSet);
    }
}

void DiscoveryClient::emitNodeDiscovered(const Node& node)
{
    if (m_nodeDiscoveredHandler)
        m_nodeDiscoveredHandler(node);
}

void DiscoveryClient::emitNodeLost(const std::string& nodeId)
{
    if (m_nodeLostHandler)
        m_nodeLostHandler(nodeId);
}

} // namespace nx::cloud::discovery