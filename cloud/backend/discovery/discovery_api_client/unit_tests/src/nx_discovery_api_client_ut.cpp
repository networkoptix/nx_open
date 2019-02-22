#include <gtest/gtest.h>

#include <memory>

#include <nx/utils/random.h>
#include <nx/cloud/discovery/discovery_api_client.h>

#include "discovery_server.h"

namespace nx::cloud::discovery::test {

namespace {

static constexpr char kClusterId[] = "TestCluster";

static std::string generateInfoJson(const std::string& nodeId)
{
    std::string random = nx::utils::random::generateName(16).toStdString();
    return std::string("{")
            + "\"nodeId\": \"" + nodeId + "\","
            "\"random\": \"" + random + "\""
        "}";
}

void sleep()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

} // namespace

class DiscoveryClient: public testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_server = std::make_unique<DiscoveryServer>(kClusterId);
        ASSERT_TRUE(m_server->bindAndListen());
    }

    void givenDiscoveryClient()
    {
        std::string nodeId = std::string("node") + std::to_string((int)m_clients.size());

        auto context = std::make_unique<ClientContext>();
        context->nodeId = nodeId;
        context->infoJson = generateInfoJson(nodeId);

        context->clusterId = kClusterId;

        context->client = std::make_unique<discovery::DiscoveryClient>(
            m_server->url(),
            context->clusterId,
            context->nodeId,
            context->infoJson);

        m_clients.emplace_back(std::move(context));
    }

    void givenDiscoveryClientAlreadyRegisteredWithServer()
    {
        givenDiscoveryClient();
        whenClientRegistersWithServer();
        thenClientRegistrationSucceeded();
    }

    void givenTwoDiscoveryClients()
    {
        for (int i = 0; i < 2; ++i)
            givenDiscoveryClient();
    }

    void givenSubscriptionToNodeDiscoveredEvent()
    {
        clientContext(0)->client->setOnNodeDiscovered(
            [this](Node node)
            {
                m_nodeDiscoveredWasTriggered = true;
                m_discoveredNode = node;
                callbackFired();
            });
    }

    void givenSubscriptionToNodeLostEvent()
    {
        clientContext(0)->client->setOnNodeLost(
            [this](const std::string& nodeId)
            {
                m_nodeLostWasTriggered = true;
                m_lostNodeId = nodeId;
                callbackFired();
            });
    }

    void whenClientUpdatesInfoJson()
    {
        auto context = clientContext(0);
        context->infoJson = generateInfoJson(context->nodeId);
        context->client->updateInformation(context->infoJson);
    }

    void thenUpdateInfoJsonSucceeded()
    {
        auto context = clientContext(0);
        while (context->client->node().infoJson != context->infoJson)
            sleep();
    }

    void whenBothClientsRegister()
    {
        for (int i = 0; i < 2; ++i)
            whenClientRegistersWithServer(i);

        // Wait for first client to become aware of second
        while (clientContext(0)->client->onlineNodes().size() != 2)
            sleep();
    }

    void whenSecondClientGoesOffline()
    {
        clientContext(1)->client.reset();
    }

    void whenWaitForNodeDiscoveredEvent()
    {
        whenClientRegistersWithServer(/*clientIndex*/ 0);
        waitForCallback();
    }

    void whenWaitForNodeLostEvent()
    {
        waitForCallback();
    }

    void whenClientRegistersWithServer(size_t clientIndex = 0)
    {
        clientContext(clientIndex)->client->start();
    }

    void thenNodeDiscoveredIsTriggered()
    {
        ASSERT_TRUE(m_nodeDiscoveredWasTriggered);
    }

    void thenNodeLostIsTriggered()
    {
        ASSERT_TRUE(m_nodeLostWasTriggered);
    }

    void thenClientRegistrationSucceeded()
    {
        thenGetOnlineNodesSucceeded();
        andOnlineNodesContainsClientNode();
    }

    void thenGetOnlineNodesSucceeded()
    {
        auto context = clientContext(/*clientIndex*/ 0);
        m_onlineNodes = context->client->onlineNodes();
        while (m_onlineNodes.empty())
        {
            sleep();
            m_onlineNodes = context->client->onlineNodes();
        }

        compareNodeAndContext(context, m_onlineNodes.front());
    }

    void andInfoJsonIsUpdated()
    {
        auto context = clientContext(/*clientIndex*/ 0);
        ASSERT_EQ(context->infoJson, context->client->node().infoJson);
    }

    void andFirstClientDoesNotHaveSecondInOnlineNodeList()
    {
        auto onlineNodes = clientContext(/*clientIndex*/ 0)->client->onlineNodes();
        while(onlineNodes.size() != 1)
        {
            sleep();
            onlineNodes = clientContext(0)->client->onlineNodes();
        }

        Node lostNode;
        lostNode.nodeId = m_lostNodeId;
        ASSERT_EQ(
            std::find(onlineNodes.begin(), onlineNodes.end(), lostNode),
            onlineNodes.end());
    }

    void andLostNodeIdMatchesExpectedId()
    {
        ASSERT_EQ(m_lostNodeId, clientContext(/*clientIndex*/ 1)->nodeId);
    }

    void andDiscoveredNodeMatchesExpectedNode()
    {
        // discovery client emits discovery event when it finds itself for the first time,
        // so compare discovered node with itself
        compareNodeAndContext(clientContext(0), m_discoveredNode);
    }

    void andOnlineNodesContainsClientNode(size_t clientIndex = 0)
    {
        auto context = clientContext(clientIndex);
        Node node = context->client->node();
        while (node.nodeId.empty())
        {
            sleep();
            node = context->client->node();
        }

        assertNodeEquality(node, m_onlineNodes.front());
    }

    void andClientIsRegistered()
    {
        andOnlineNodesContainsClientNode();
    }

private:
    struct ClientContext
    {
        std::string clusterId;
        std::string nodeId;
        std::string infoJson;
        std::unique_ptr<discovery::DiscoveryClient> client;
    };

    ClientContext* clientContext(size_t index)
    {
        return m_clients[index].get();
    }

    void assertNodeEquality(const Node&a, const Node& b)
    {
        ASSERT_EQ(a.nodeId, b.nodeId);
        ASSERT_EQ(a.host, b.host);
        ASSERT_EQ(a.infoJson, b.infoJson);
        // expirationTime is volatile, so don't compare because it changes
    }

    void compareNodeAndContext(ClientContext* clientContext, const Node& node)
    {
        ASSERT_EQ(node.nodeId, clientContext->nodeId);
        ASSERT_EQ(node.infoJson, clientContext->infoJson);
        ASSERT_FALSE(node.host.empty());

        auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(
            node.expirationTime.time_since_epoch());

        ASSERT_TRUE(timeSinceEpoch > std::chrono::milliseconds::zero());
    }

    Node waitForClientRegistration(size_t clientIndex = 0)
    {
        auto context = clientContext(clientIndex);
        Node node = context->client->node();
        while (node.nodeId != context->nodeId)
        {
            sleep();
            node = context->client->node();
        }
        return node;
    }

    void waitForCallback()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wait.wait(lock, [this]() { return m_callbackFired.load(); });
        m_callbackFired = false;
    }

    void callbackFired()
    {
        m_callbackFired = true;
        m_wait.notify_all();
    }

private:
    std::unique_ptr<DiscoveryServer> m_server;
    std::vector<std::unique_ptr<ClientContext>> m_clients;

    std::mutex m_mutex;
    std::condition_variable m_wait;
    std::atomic_bool m_callbackFired = false;

    std::vector<Node> m_onlineNodes;

    Node m_discoveredNode;
    std::string m_lostNodeId;

    bool m_nodeDiscoveredWasTriggered = false;
    bool m_nodeLostWasTriggered = false;
};

TEST_F(DiscoveryClient, retrieves_onlines_nodes_including_self)
{
    givenDiscoveryClient();

    whenClientRegistersWithServer();

    thenGetOnlineNodesSucceeded();

    andOnlineNodesContainsClientNode();
}

TEST_F(DiscoveryClient, registers_with_discovery_service)
{
    givenDiscoveryClient();

    whenClientRegistersWithServer();

    thenClientRegistrationSucceeded();

    andClientIsRegistered();
}

TEST_F(DiscoveryClient, provides_node_discovered_event)
{
    givenDiscoveryClient();
    givenSubscriptionToNodeDiscoveredEvent();

    whenWaitForNodeDiscoveredEvent();

    thenNodeDiscoveredIsTriggered();

    andDiscoveredNodeMatchesExpectedNode();
}

TEST_F(DiscoveryClient, provides_node_lost_event)
{
    givenTwoDiscoveryClients();
    givenSubscriptionToNodeLostEvent();

    whenBothClientsRegister();
    whenSecondClientGoesOffline();
    whenWaitForNodeLostEvent();

    thenNodeLostIsTriggered();

    andLostNodeIdMatchesExpectedId();
    andFirstClientDoesNotHaveSecondInOnlineNodeList();
}

TEST_F(DiscoveryClient, updates_json_information)
{
    givenDiscoveryClientAlreadyRegisteredWithServer();

    whenClientUpdatesInfoJson();

    thenUpdateInfoJsonSucceeded();

    andInfoJsonIsUpdated();
}

} // namespace nx::cloud::discovery::test