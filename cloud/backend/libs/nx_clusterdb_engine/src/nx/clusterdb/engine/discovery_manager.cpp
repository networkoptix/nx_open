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
    if (!m_discoverySettings.enabled)
        NX_VERBOSE(this, "Discovery is disabled in settings. calling start() will do nothing");
}

void DiscoveryManager::start(
    const std::string& clusterId,
    const nx::utils::Url& synchronizationEngineUrl)
{
    if (!m_discoverySettings.enabled)
        return;

    if (m_discoveryClient)
        return;

    if (clusterId.empty())
    {
        NX_WARNING(this, "Empty clusterId provided, discovery will not start");
        return;
    }

    m_discoveryClient = std::make_unique<nx::cloud::discovery::DiscoveryClient>(
        m_discoverySettings,
        clusterId,
        buildNodeInfo(synchronizationEngineUrl));

    m_discoveryClient->setOnNodeDiscovered(
        [this, clusterId](nx::cloud::discovery::Node node)
        {
            if (node.urls.empty())
            {
                NX_WARNING(this,
                    "Discovered node %1 provided 0 urls, can't connect to remote synchronization engine",
                    toString(node));
                return;
            }

            NX_VERBOSE(this, "Discovered a new node: %1", toString(node));

            m_syncEngine->connector().addNodeUrl(clusterId, node.urls.front());
        });

    m_discoveryClient->start();

    NX_VERBOSE(this, "discovery started with cluster id: %1 on url: %2",
        clusterId, synchronizationEngineUrl.toStdString());
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

void DiscoveryManager::pleaseStopSync()
{
    if (m_discoveryClient)
        m_discoveryClient->pleaseStopSync();
}

nx::cloud::discovery::NodeInfo DiscoveryManager::buildNodeInfo(
    const nx::utils::Url& synchronizationEngineUrl) const
{
    return nx::cloud::discovery::NodeInfo{
        /*nodeId*/ m_syncEngine->peerId().toSimpleString().toStdString(),
        /*urls*/ {synchronizationEngineUrl.toStdString()},
        /*infoJson*/ {}
    };
}

} //namespace nx::clusterdb::engine
