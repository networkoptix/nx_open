#include "discovery_api_client.h"

#include <nx/utils/timer_holder.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/buffer_source.h>

#include "request_paths.h"

namespace nx::cloud::discovery {

//-------------------------------------------------------------------------------------------------
// RequestContext

DiscoveryClient::RequestContext::RequestContext(ResponseReceivedHandler responseReceived)
{
    m_httpClient.setOnDone(
        [this, responseReceived = std::move(responseReceived)]()
        {
            auto messageBody = m_httpClient.fetchMessageBodyBuffer();
            responseReceived(std::move(messageBody));
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
    const nx::network::http::BufferType& messageBody)
{
    m_httpClient.setRequestBody(
        std::make_unique<nx::network::http::BufferSource>("application/json", messageBody));
    m_httpClient.doPost(url);
}

bool DiscoveryClient::RequestContext::failed() const
{
    return m_httpClient.failed();
}

SystemError::ErrorCode DiscoveryClient::RequestContext::lastSysErrorCode() const
{
    return m_httpClient.lastSysErrorCode();
}

const nx::network::http::Request& DiscoveryClient::RequestContext::request() const
{
    return m_httpClient.request();
}

const nx::network::http::Response* DiscoveryClient::RequestContext::response() const
{
    return m_httpClient.response();
}

//-------------------------------------------------------------------------------------------------
// DiscoveryClient

DiscoveryClient::DiscoveryClient(
    const Settings& discoverySettings,
    const std::string& clusterId,
    const NodeInfo& nodeInfo)
    :
    m_settings(discoverySettings),
    m_thisNodeInfo(nodeInfo)
{
    NX_ASSERT(m_settings.roundTripPadding >= std::chrono::milliseconds::zero());

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
    m_registerNodeRequest->bindToAioThread(aioThread);
    m_onlineNodesRequest->bindToAioThread(aioThread);
}

void DiscoveryClient::start()
{
    startRegisterNodeRequest(std::chrono::milliseconds::zero());
    startOnlineNodesRequest(std::chrono::milliseconds::zero());
}

void DiscoveryClient::updateInformation(const std::string& infoJson)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_thisNodeInfo.infoJson = infoJson;
    }

    m_registerNodeRequest->cancelSync();

    startRegisterNodeRequest(std::chrono::milliseconds::zero());
}

std::vector<Node> DiscoveryClient::onlineNodes() const
{
    QnMutexLocker lock(&m_mutex);
    return std::vector<Node>(m_onlineNodes.begin(), m_onlineNodes.end());
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
    m_discoveryServiceUrl = m_settings.discoveryServiceUrl;
    m_discoveryServiceUrl.setPath(
        nx::network::http::rest::substituteParameters(
            kRegisterNodePath,
            {clusterId}).c_str());
}

const nx::utils::Url& DiscoveryClient::discoveryServiceUrl() const
{
    return m_discoveryServiceUrl;
}

void DiscoveryClient::setupRegisterNodeRequest()
{
    m_registerNodeRequest = std::make_unique<RequestContext>(
        [this](nx::network::http::BufferType messageBody)
        {
            if (m_registerNodeRequest->failed())
            {
                NX_WARNING(this, lm("Failed to connect to discovery server at: %1, error: %2")
                    .arg(discoveryServiceUrl())
                    .arg(SystemError::toString(m_registerNodeRequest->lastSysErrorCode())));
                startRegisterNodeRequest(m_settings.registrationErrorDelay);
                return;
            }

            if (!nx::network::http::StatusCode::isSuccessCode(
                m_registerNodeRequest->response()->statusLine.statusCode))
            {
                NX_WARNING(this, lm("Request to register node failed: %1")
                    .arg(m_registerNodeRequest->response()->statusLine.toString()));
                startRegisterNodeRequest(m_settings.registrationErrorDelay);
                return;
            }

            bool ok = false;
            Node node = NodeSerialization::deserialized(messageBody, Node(), &ok);
            if (ok)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    m_thisNode = std::move(node);
                }
                NX_VERBOSE(this, lm("Received registration response: %1").arg(messageBody));
            }
            else
            {
                NX_WARNING(this, lm("Error deserializing register node response: %1")
                    .arg(messageBody));
            }

            auto registrationDelay = calculateRegistrationDelay(m_registerNodeRequest->response());
            startRegisterNodeRequest(
                registrationDelay.has_value()
                    ? *registrationDelay
                    : m_settings.registrationErrorDelay);
        });
}

void DiscoveryClient::setupOnlineNodesRequest()
{
    m_onlineNodesRequest = std::make_unique<RequestContext>(
        [this](nx::network::http::BufferType messageBody)
        {
            // All error logging related to server failure is done by lambda in
            // setupRegisterNodeRequest.
            if (m_onlineNodesRequest->failed())
            {
                startOnlineNodesRequest(m_settings.onlineNodesRequestDelay);
                return;
            }

            if (!nx::network::http::StatusCode::isSuccessCode(
                m_onlineNodesRequest->response()->statusLine.statusCode))
            {
                startOnlineNodesRequest(m_settings.onlineNodesRequestDelay);
                return;
            }

            bool ok = false;
            auto onlineNodes =
                NodeSerialization::deserialized(messageBody, std::vector<Node>(), &ok);
            if (ok)
            {
                updateOnlineNodes(std::move(onlineNodes));
            }
            else
            {
                NX_WARNING(this, lm("Error deserializing online nodes response: %1")
                    .arg(messageBody));
            }

            startOnlineNodesRequest(m_settings.onlineNodesRequestDelay);
        });
}

void DiscoveryClient::startRegisterNodeRequest(const std::chrono::milliseconds& delay)
{
    static constexpr char kRequestMessageTemplate[] =
        "Registration request made at %1."
        " Node expiration time is %2.";

    m_registerNodeRequest->startTimer(
        delay,
        [this]()
        {
            QnMutexLocker lock(&m_mutex);
            m_registerNodeRequest->doPost(
                discoveryServiceUrl(),
                QJson::serialized(m_thisNodeInfo));

            updateRequestSentTime(m_registerNodeRequest->request());

            // dt will be invalid the first time registration is done.
            QDateTime dt = toDateTime(m_thisNode.expirationTime);
            QString expirationTime = dt.isValid() ? dt.toString() : "now";

            NX_VERBOSE(this, lm(kRequestMessageTemplate)
                    .arg(m_requestSent.toString())
                    .arg(expirationTime));
        });
}

void DiscoveryClient::startOnlineNodesRequest(const std::chrono::milliseconds& delay)
{
    m_onlineNodesRequest->startTimer(
        delay,
        [this]()
        {
            m_onlineNodesRequest->doGet(discoveryServiceUrl());
        });
}

void DiscoveryClient::updateOnlineNodes(std::vector<Node> onlineNodes)
{
    std::set<Node> onlineNodeSet;

    std::move(
        onlineNodes.begin(),
        onlineNodes.end(),
        std::inserter(onlineNodeSet, onlineNodeSet.end()));

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
    {
        // don't emit discovery event for "this" node.
        // Clients that rely on discovery event might try to connect to themselves.
        if (node.nodeId != m_thisNodeInfo.nodeId)
            emitNodeDiscovered(node);
    }

    for (const auto& node : lostNodes)
        emitNodeLost(node);
}

void DiscoveryClient::emitNodeDiscovered(const Node& node)
{
    if (m_nodeDiscoveredHandler)
        m_nodeDiscoveredHandler(node);
}

void DiscoveryClient::emitNodeLost(const Node& node)
{
    if (m_nodeLostHandler)
        m_nodeLostHandler(node);
}

std::optional<QDateTime> DiscoveryClient::getServerResponseTime(
    const nx::network::http::Response* response) const
{
    if (!response)
        return std::nullopt;

    auto it = response->headers.find("Date");
    if (it == response->headers.end())
    {
        NX_WARNING(
            this,
            lm("http response does not contain 'Date' http header, using current time."));
        return std::nullopt;
    }

    auto responseTime = nx::network::http::parseDate(it->second);
    if (responseTime.isValid())
        return responseTime;

    NX_WARNING(
        this,
        lm("Error parsing Date header in http response: received %1. Using current time.")
            .arg(it->second));

    return std::nullopt;
}

void DiscoveryClient::updateRequestSentTime(const nx::network::http::Request& request)
{
    auto it = request.headers.find("Date");
    if (it == request.headers.end())
    {
        NX_WARNING(
            this,
            lm("http request does not contain 'Date' http header, using current time."));
        m_requestSent = QDateTime::currentDateTimeUtc();
        return;
    }

    m_requestSent = nx::network::http::parseDate(it->second);
    if (m_requestSent.isValid())
        return;

    NX_WARNING(
        this,
        lm("Error parsing date from http request. Format given by request is: %1. Using current time.")
            .arg(it->second));
    m_requestSent = QDateTime::currentDateTimeUtc();
}

std::optional<std::chrono::milliseconds>
DiscoveryClient::calculateRegistrationDelay(const nx::network::http::Response* response)
{
    using namespace std::chrono;

    static constexpr char kNegativeDelayErrorTemplate[] =
        "Calculated a negative request delay using the following values:"
        "node expiration time ET (msec since epoch) = %1,  "
        "request travel time to server RTT (in msecs) = %2,  "
        "time at which request delay was calculated N (msec since epoch) = %3,  "
        "ET - N - RTT = %4. Using zero instead.";

    auto serverResponseTime = getServerResponseTime(response);
    if (!serverResponseTime.has_value())
        return std::nullopt;

    milliseconds travelTime(
        (m_requestSent.msecsTo(*serverResponseTime)
         + (*serverResponseTime).msecsTo(QDateTime::currentDateTimeUtc())) / 2);

    if (travelTime < milliseconds::zero())
    {
        NX_WARNING(this,
            lm("Error calculated %1 msec travel time from client to server. Using zero delay instead.")
            .arg(travelTime.count()));
        return std::nullopt;
    }

    auto now = system_clock::now();

    auto registrationDelay =
        duration_cast<milliseconds>(m_thisNode.expirationTime - now - travelTime);

    if (registrationDelay < milliseconds::zero())
    {
        NX_WARNING(this, lm(kNegativeDelayErrorTemplate)
            .arg(duration_cast<milliseconds>(
                m_thisNode.expirationTime.time_since_epoch()).count())
            .arg(travelTime.count())
            .arg(duration_cast<milliseconds>(now.time_since_epoch()).count())
            .arg(registrationDelay.count()));
        return std::nullopt;
    }

    return registrationDelay;
}

} // namespace nx::cloud::discovery
