#include "discovery_manager.h"

#include <nx/network/http/http_types.h>

#include "synchronization_engine.h"

namespace nx::clusterdb::engine{

namespace {

static QString toString(const nx::cloud::discovery::Node& node)
{
    return
        QString("{ nodeId: ") + node.nodeId.c_str()
        + ", host: " + node.host.c_str()
        + ", expirationTime" + nx::network::http::formatDateTime(
            nx::cloud::discovery::toDateTime(node.expirationTime))
        + ", infoJson: " + node.infoJson.c_str() + " }";
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
            m_syncEngine->connector().addNodeUrl(clusterId, node.host);
        });

    m_discoveryClient->setOnNodeLost(
        [this, clusterId](nx::cloud::discovery::Node node)
        {
            m_syncEngine->connector().removeNodeUrl(clusterId, node.host);
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
