#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <string>

#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_client.h>

#include <nx/utils/timer_holder.h>
#include <nx/utils/timer_manager.h>

namespace nx::cloud::discovery {

struct Node
{
    enum Status
    {
        online,
        offline
    };

    std::string nodeId;
    std::string host;
    /* http time format */
    std::string expirationTime;
    /* The object under "info" key */
    std::string infoJson;
};

#define Node_Fields (nodeId)(host)(expirationTime)(infoJson)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Node),
    (json))

//-------------------------------------------------------------------------------------------------

using NodeDiscoveredHandler = nx::utils::MoveOnlyFunc<void(Node, Node::Status)>;

using NodeLostHandler = nx::utils::MoveOnlyFunc<void(std::string /*nodeId*/)>;

/**
  * Updating node registration based on expiration timer is handled by this class.
  * Situations, where we can't reach the discovery service, are handled by this class too.
  * NOTE: Every method is non-blocking.
  */
class DiscoveryClient:
    public nx::network::aio::BasicPollable
{
public:
    DiscoveryClient(
        const nx::utils::Url& discoveryServiceUrl,
        const std::string& clusterId,
        const std::string& nodeId,
        const std::string& infoJson);

    /** Starts the process of registering node in the discovery service. */
    void start();

    /**
     * This information is sent to the discovery node ASAP.
     */
    void updateInformation(const std::string& infoJson);

    std::vector<Node> onlineNodes() const;

    void setOnNodeDiscovered(NodeDiscoveredHandler handler);
    void setOnNodeLost(NodeLostHandler handler);

private:
    nx::utils::Url m_discoveryServiceUrl;
    std::string m_clusterId;
    Node m_node;

    NodeDiscoveredHandler m_nodeDiscoveredHandler;
    NodeLostHandler m_nodeLostHandler;

    nx::network::http::HttpClient m_httpClient;
};

} // namespace nx::cloud::discovery