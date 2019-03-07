#pragma once

#include <nx/cloud/discovery/discovery_api_client.h>

namespace nx::clusterdb::engine {

class SynchronizationEngine;

/**
 * Uses nx::cloud::discovery::DiscoveryClient to auto discover other clusterdb engine nodes
 * and automatically connects to them.
 */
class NX_DATA_SYNC_ENGINE_API DiscoveryManager
{
public:
    DiscoveryManager(
        const nx::cloud::discovery::Settings& discoverySettings,
        SynchronizationEngine* syncEngine);

    /**
     * Starts discovery service.
     * @clusterId the cluster id that this node should advertize under and discover other nodes.
     * @synchronizationEngineUrl the url that the synchronization engine listens on.
     */
    void start(
        const std::string& clusterId,
        const nx::utils::Url& synchronizationEngineUrl);

    /**
     * Update this node's information, sending to discovery service ASAP.
     * NOTE: Does nothing if start() hasn't been called, or if stop() has been called.
     */
    void updateInformation(const std::string& infoJson);

    /**
     * Get the underlying discovery client. Returns nullptr if start() hasn't been called.
     */
    const nx::cloud::discovery::DiscoveryClient* discoveryClient() const;

private:
    nx::cloud::discovery::NodeInfo buildNodeInfo(
        const nx::utils::Url& synchronizationEngineUrl) const;

private:
    const nx::cloud::discovery::Settings& m_discoverySettings;
    SynchronizationEngine* m_syncEngine;

    std::unique_ptr<nx::cloud::discovery::DiscoveryClient> m_discoveryClient;
};

} //namespace nx::clusterdb::engine
