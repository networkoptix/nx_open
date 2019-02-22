#include "discovery_api_client.h"

#include <nx/utils/timer_holder.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/buffer_source.h>

#include "request_paths.h"

namespace nx::cloud::discovery {

namespace {

static const std::chrono::milliseconds kNow(0);

static std::set<Node> moveToSet(std::vector<Node>& nodes)
{
    std::set<Node> nodeSet;
    for (auto& node : nodes)
        nodeSet.emplace(std::move(node));
    return nodeSet;
}

static std::vector<Node> toVector(const std::set<Node>& nodes)
{
    std::vector<Node> nodeList;
    nodeList.reserve(nodes.size());
    std::transform(nodes.begin(), nodes.end(),
        std::back_inserter(nodeList), [](auto node) { return node; });
    return nodeList;
}

} //namespace

//-------------------------------------------------------------------------------------------------
// RequestContext

DiscoveryClient::RequestContext::RequestContext(
    ResponseReceivedHandler responseReceived)
{
    m_httpClient.setOnDone(
        [this, responseReceived = std::move(responseReceived)]()
        {
            m_messageBody = m_httpClient.fetchMessageBodyBuffer();
            responseReceived(std::exchange(m_messageBody, {}));
        });
}

void DiscoveryClient::RequestContext::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    m_timer.bindToAioThread(aioThread);
    m_httpClient.bindToAioThread(aioThread);
}

void DiscoveryClient::RequestContext::pleaseStopSync()
{
    m_httpClient.pleaseStopSync();
    m_timer.pleaseStopSync();
}

void DiscoveryClient::RequestContext::startTimer(
    const std::chrono::milliseconds& timeout,
    nx::network::aio::TimerEventHandler timerEvent)
{
    m_timer.start(timeout, std::move(timerEvent));
}

void DiscoveryClient::RequestContext::stopTimerSync()
{
    m_timer.cancelSync();
}

void DiscoveryClient::RequestContext::cancelSync()
{
    m_httpClient.cancelPostedCallsSync();
    m_timer.cancelSync();
}

void DiscoveryClient::RequestContext::doGet(const nx::utils::Url& url)
{
    m_httpClient.doGet(url);
}

void DiscoveryClient::RequestContext::doPost(
    const nx::utils::Url& url,
    const QByteArray& messageBody)
{
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
    setupRegisterNodeRequest();
    setupOnlineNodesRequest();

    bindToAioThread(getAioThread());
}

DiscoveryClient::~DiscoveryClient()
{
    pleaseStopSync();
}

void DiscoveryClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_registerNodeRequest->bindToAioThread(getAioThread());
    m_onlineNodesRequest->bindToAioThread(getAioThread());
}

void DiscoveryClient::start()
{
    startRegisterNodeRequest();
    startOnlineNodesRequest();
}

void DiscoveryClient::updateInformation(const std::string& infoJson)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_thisNodeInfo.infoJson = infoJson;
    }

    m_registerNodeRequest->cancelSync();

    startRegisterNodeRequest();
}

std::vector<Node> DiscoveryClient::onlineNodes() const
{
    QnMutexLocker lock(&m_mutex);
    return toVector(m_onlineNodes);
}

Node DiscoveryClient::node() const
{
    QnMutexLocker lock(&m_mutex);
    return m_thisNode;
}

void DiscoveryClient::setOnNodeDiscovered(NodeDiscoveredHandler handler)
{
    m_nodeDiscoveredHandler = std::move(handler);
}

void DiscoveryClient::setOnNodeLost(NodeLostHandler handler)
{
    m_nodeLostHandler = std::move(handler);
}

void DiscoveryClient::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_registerNodeRequest->pleaseStopSync();
    m_onlineNodesRequest->pleaseStopSync();
}

void DiscoveryClient::setupDiscoveryServiceUrl(const std::string& clusterId)
{
    QString nodeRegistryPath(kRegisterNodePath);
    nodeRegistryPath.replace(kClusterIdTemplate, clusterId.c_str());
    m_discoveryServiceUrl.setPath(nodeRegistryPath);
}

void DiscoveryClient::setupRegisterNodeRequest()
{
    m_registerNodeRequest = std::make_unique<RequestContext>(
        [this](QByteArray messageBody)
        {
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
}

void DiscoveryClient::setupOnlineNodesRequest()
{
    m_onlineNodesRequest = std::make_unique<RequestContext>(
        [this](QByteArray messageBody)
        {
            bool ok = false;
            auto onlineNodes = QJson::deserialized(messageBody, std::vector<Node>(), &ok);
            if (ok)
                updateOnlineNodes(onlineNodes);
            else
                NX_ERROR(this, lm("Error deserializing online nodes response"));

            startOnlineNodesRequest();
        });
}

void DiscoveryClient::startRegisterNodeRequest()
{
    m_registerNodeRequest->startTimer(
        std::chrono::milliseconds::zero(),
        [this]()
        {
            QnMutexLocker lock(&m_mutex);
            m_registerNodeRequest->doPost(
                m_discoveryServiceUrl,
                QJson::serialized(m_thisNodeInfo));
        });
}

void DiscoveryClient::startOnlineNodesRequest()
{
    m_onlineNodesRequest->startTimer(
        std::chrono::milliseconds::zero(),
        [this]()
        {
            m_onlineNodesRequest->doGet(m_discoveryServiceUrl);
        });
}

void DiscoveryClient::updateOnlineNodes(std::vector<Node>& onlineNodes)
{
    auto onlineNodeSet = moveToSet(onlineNodes);

    std::vector<Node> discoveredNodes;
    std::vector<Node> lostNodes;

    // Nodes that are not present in onlineNodeSet and not m_onlineNodes are newly online.
    for (const auto& node : onlineNodeSet)
    {
        if (m_onlineNodes.find(node) == m_onlineNodes.end())
            discoveredNodes.emplace_back(node);
    }

    // Nodes that are present in m_onlineNodes and not in onlineNodeSet have been lost.
    for (const auto& node : m_onlineNodes)
    {
        if (onlineNodeSet.find(node) == onlineNodeSet.end())
            lostNodes.emplace_back(node);
    }

    // Update m_onlineNodes before emitting events, as events might access onlineNodes()
    {
        QnMutexLocker lock(&m_mutex);
        m_onlineNodes = std::move(onlineNodeSet);
    }

    for (const auto& node : discoveredNodes)
        emitNodeDiscovered(node);

    for (const auto& node : lostNodes)
        emitNodeLost(node.nodeId);
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