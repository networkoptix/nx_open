#include "discovery_api_client.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::discovery {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Node),
    (json),
    _Fields)

//-------------------------------------------------------------------------------------------------

DiscoveryClient::DiscoveryClient(
    const nx::utils::Url& discoveryServiceUrl,
    const std::string& clusterId,
    const std::string& nodeId,
    const std::string& infoJson)
    :
    m_discoveryServiceUrl(discoveryServiceUrl),
    m_clusterId(clusterId)
{
    m_node.nodeId = nodeId;
    m_node.infoJson = infoJson;
}

void DiscoveryClient::start()
{
    m_httpClient.doPost(
        m_discoveryServiceUrl,
        "application/json",
        QJson::serialized<Node>(m_node));
}

void DiscoveryClient::updateInformation(const std::string& infoJson)
{
    m_node.infoJson = infoJson;
    start();
}

std::vector<Node> DiscoveryClient::onlineNodes() const
{
    return std::vector<Node>();
}

void DiscoveryClient::setOnNodeDiscovered(NodeDiscoveredHandler handler)
{
    m_nodeDiscoveredHandler = std::move(handler);
}

void DiscoveryClient::setOnNodeLost(NodeLostHandler handler)
{
    m_nodeLostHandler = std::move(handler);
}

} //namespace nx::cloud::discovery