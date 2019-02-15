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
{
}

void DiscoveryClient::start()
{
}

void DiscoveryClient::updateInformation(const std::string& infoJson)
{
}

std::vector<Node> DiscoveryClient::onlineNodes() const
{
    return std::vector<Node>();
}

void DiscoveryClient::setOnNodeDiscovered(NodeDiscoveredHandler handler)
{
}

void DiscoveryClient::setOnNodeLost(NodeLostHandler handler)
{
}



} //namespace nx::cloud::discovery