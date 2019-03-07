#include "discovery_manager.h"

#include <nx/network/http/http_types.h>

#include "synchronization_engine.h"

namespace nx::clusterdb::engine{

namespace {

static QString toString(const nx::cloud::discovery::Node& node)
{
    return nx::cloud::discovery::NodeSerialization::serialized(node);
}

bool isHttpScheme(const nx::utils::Url& url)
{
    return url.scheme() == nx::network::http::kUrlSchemeName;
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
            NX_VERBOSE(
                this,
                lm("Discovered a new node: %1. all non http scheme urls will be ignored.")
                    .arg(toString(node)));

            for (const auto& urlString : node.urls)
            {
                nx::utils::Url url(urlString);
                if (isHttpScheme(url))
                    m_syncEngine->connector().addNodeUrl(clusterId, url);
            }
        });

    m_discoveryClient->setOnNodeLost(
        [this, clusterId](nx::cloud::discovery::Node node)
        {
            NX_VERBOSE(this, lm("Disconnecting from lost node: %1").arg(toString(node)));

            for(const auto url: node.urls)
                m_syncEngine->connector().removeNodeUrl(clusterId, node.urls.front());
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
