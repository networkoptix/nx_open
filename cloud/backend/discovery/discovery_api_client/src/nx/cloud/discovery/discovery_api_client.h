#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <set>
#include <vector>
#include <string>

#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_async_client.h>

#include <nx/network/aio/timer.h>

namespace nx::cloud::discovery {

struct NodeInfo
{
    std::string nodeId;
    /* The object under "info" key */
    std::string infoJson;
};

#define NodeInfo_Fields (nodeId)(infoJson)

struct Node
{
    std::string nodeId;
    std::string host;
    /* http time format */
    std::chrono::system_clock::time_point expirationTime;
    /* The object under "info" key */
    std::string infoJson;

    bool operator==(const Node& right) const
    {
        // TODO do other fields need to be compared? they are volatile.
        return nodeId == right.nodeId;
    }

    bool operator<(const Node& right)const
    {
        return nodeId < right.nodeId;
    }
};

#define Node_Fields (nodeId)(host)(expirationTime)(infoJson)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (NodeInfo)(Node),
    (json))

//-------------------------------------------------------------------------------------------------

using NodeDiscoveredHandler = nx::utils::MoveOnlyFunc<void(Node)>;

using NodeLostHandler = nx::utils::MoveOnlyFunc<void(std::string /*nodeId*/)>;

/**
  * Updating node registration based on expiration timer is handled by this class.
  * Situations, where we can't reach the discovery service, are handled by this class too.
  * NOTE: Every method is non-blocking.
  */
class NX_DISCOVERY_CLIENT_API DiscoveryClient:
    public nx::network::aio::BasicPollable
{
    using BaseType = nx::network::aio::BasicPollable;

public:
    DiscoveryClient(
        const nx::utils::Url& discoveryServiceUrl,
        const std::string& clusterId,
        const std::string& nodeId,
        const std::string& infoJson);
    ~DiscoveryClient();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;


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
    void setupDiscoveryServiceUrl(const std::string& clusterId);

    void startRegisterNode();
    void startOnlineNodesRequest();

    void updateOnlineNodes(const std::vector<Node>& discoveredNodes);

    void emitNodeDiscovered(const Node& node);
    void emitNodeLost(const std::string& nodeId);

private:
    using ResponseReceivedHandler = nx::utils::MoveOnlyFunc<void(QByteArray messageBody)>;
    struct RequestContext
    {
        RequestContext(ResponseReceivedHandler responseReceived);

        void bindToAioThread(
            nx::network::aio::AbstractAioThread* aioThread);
        void stopWhileInAioThread();

        void start(nx::network::aio::TimerEventHandler timerEvent);
        void stop();

        void doGet(const nx::utils::Url& url);
        void doPost(const nx::utils::Url& url, const QByteArray& messagBody);

    private:
        void clearMessageBody();
        void updateMessageBody();

    private:
        QByteArray m_messageBody;
        nx::network::http::AsyncClient m_httpClient;
        nx::network::aio::Timer m_timer;
    };

    nx::utils::Url m_discoveryServiceUrl;
    Node m_thisNode;
    Node m_thisNodeInfo;

    NodeDiscoveredHandler m_nodeDiscoveredHandler;
    NodeLostHandler m_nodeLostHandler;

    std::unique_ptr<RequestContext> m_registerNodeRequest;
    std::unique_ptr<RequestContext> m_onlineNodesRequest;

    std::set<Node> m_onlineNodes;

    mutable QnMutex m_mutex;
};

} // namespace nx::cloud::discovery