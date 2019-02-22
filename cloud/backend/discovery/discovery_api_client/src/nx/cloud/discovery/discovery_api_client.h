#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <set>
#include <vector>
#include <string>

#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>
#include <nx/network/http/http_async_client.h>

#include <nx/network/aio/timer.h>

#include "node.h"

namespace nx::cloud::discovery {

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
    using base_type = nx::network::aio::BasicPollable;

public:
    DiscoveryClient(
        const nx::utils::Url& discoveryServiceUrl,
        const std::string& clusterId,
        const std::string& nodeId,
        const std::string& infoJson);
    ~DiscoveryClient();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    /** Starts the process of registering node in the discovery service. */
    void start();

    /**
     * This information is sent to the discovery node ASAP.
     */
    void updateInformation(const std::string& infoJson);

    /**
     * Get the list of nodes registered with the server, including the Node returned by node()
     */
    std::vector<Node> onlineNodes() const;

    /**
     * Get the Node representing this DiscoveryClient.
     */
    Node node() const;

    void setOnNodeDiscovered(NodeDiscoveredHandler handler);
    void setOnNodeLost(NodeLostHandler handler);

private:
    virtual void stopWhileInAioThread() override;

    void setupDiscoveryServiceUrl(const std::string& clusterId);
    void setupRegisterNodeRequest();
    void setupOnlineNodesRequest();

    void startRegisterNodeRequest();
    void startOnlineNodesRequest();

    void updateOnlineNodes(std::vector<Node>& discoveredNodes);

    void emitNodeDiscovered(const Node& node);
    void emitNodeLost(const std::string& nodeId);

private:
    using ResponseReceivedHandler = nx::utils::MoveOnlyFunc<void(QByteArray messageBody)>;
    class RequestContext
    {
    public:
        RequestContext(ResponseReceivedHandler responseReceived);

        void bindToAioThread(
            nx::network::aio::AbstractAioThread* aioThread);
        void pleaseStopSync();

        void startTimer(
            const std::chrono::milliseconds& timeout,
            nx::network::aio::TimerEventHandler timerEvent);

        void cancelSync();

        void doGet(const nx::utils::Url& url);
        void doPost(const nx::utils::Url& url, const QByteArray& messagBody);

    private:
        QByteArray m_messageBody;
        nx::network::http::AsyncClient m_httpClient;
        nx::network::aio::Timer m_timer;

        std::mutex m_mutex;
        std::condition_variable m_wait;
        std::atomic_bool m_ready = false;
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