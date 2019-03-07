#include "discovery_manager.h"

#include <nx/network/http/http_types.h>

#include "synchronization_engine.h"

namespace nx::clusterdb::engine{

namespace {

static QString toString(const nx::cloud::discovery::Node& node)
{
    return nx::cloud::discovery::NodeSerialization::serialized(node);
}

} // namespace

DiscoveryManager::DiscoveryManager(
    const nx::cloud::discovery::Settings& discoverySettings,
    SynchronizationEngine* syncEngine)
    :
    m_discoverySettings(discoverySettings),
    m_syncEngine(syncEngine)
{
}

void DiscoveryManager::start(
    const std::string& clusterId,
    const nx::cloud::discovery::NodeInfo& nodeInfo)
{
    if (m_discoveryClient)
        return;

    m_discoveryClient = std::make_unique<nx::cloud::discovery::DiscoveryClient>(
        m_discoverySettings,
        clusterId,
        nodeInfo);

    m_discoveryClient->setOnNodeDiscovered(
        [this, clusterId](nx::cloud::discovery::Node node)
        {
            if (node.urls.empty())
            {
                NX_WARNING(
                    this,
                    lm("Node %1 discovered with an empty url list! Skipping sync engine connection.")
                    .arg(node.nodeId.c_str()));
                return;
            }

            NX_INFO(
                this,
                lm("Discovered node %1 at url: %2. Connecting sync engine to it.")
                    .arg(node.nodeId).arg(node.urls.front().c_str()));
            // Synchronization engine provides only one url.
            m_syncEngine->connector().addNodeUrl(clusterId, node.urls.front());
        });

    m_discoveryClient->setOnNodeLost(
        [this, clusterId](nx::cloud::discovery::Node node)
        {
            if (!node.urls.empty())
            {
                NX_INFO(
                    this,
                    lm("Disconnecting from lost node %1 at url:")
                        .arg(node.nodeId.c_str()).arg(node.urls.front()));
            }
        });

    m_discoveryClient->start();
}

void DiscoveryManager::stop()
{
    m_discoveryClient.reset();
}

void DiscoveryManager::updateInformation(const std::string& infoJson)
{
    if (m_discoveryClient)
        m_discoveryClient->updateInformation(infoJson);
}

const nx::cloud::discovery::DiscoveryClient* DiscoveryManager::discoveryClient() const
{
    return m_discoveryClient.get();
}

} //namespace nx::clusterdb::engine
