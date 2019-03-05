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
     * @clusterId the id of the cluster to register with.
     * @ nodeInfo the initial node information to register.
     */
    void start(
        const std::string& clusterId,
        const nx::cloud::discovery::NodeInfo& nodeInfo);

    /**
     * Stops the discovery service;
     */
    void stop();

    /**
     * Update this node's information, sending to discovery service ASAP.
     * NOTE: Does nothing if start() hasn't been called, or if stop() has been called.
     */
    void updateInformation(const std::string& infoJson);

    /**
     * Get the underlying discovery client.
     *
     * @return the underyling discovery client, or nullptr if either start() hasn't been called
     * or stop() was called.
     */
    const nx::cloud::discovery::DiscoveryClient* discoveryClient() const;

private:
    const nx::cloud::discovery::Settings& m_discoverySettings;
    SynchronizationEngine* m_syncEngine;

    std::unique_ptr<nx::cloud::discovery::DiscoveryClient> m_discoveryClient;
};

} //namespace nx::clusterdb::engine
