#pragma once

#include "cloud_storage_launcher.h"

namespace nx::cloud::storage::service::test {

/**
 * Convenience class for launching CloudStorage instances in cluster mode.
 * NOTE: Owns each instance that is added to the cluster.
 */
class CloudStorageCluster
{
public:
    CloudStorageCluster(const nx::utils::Url& discoveryServiceUrl);
    ~CloudStorageCluster();

    /**
     * Adds a CloudStorageLauncher to the cluster without starting it, so additional args
     * can be passed before starting the instance.
     * NOTE: CloudStorageLauncher::startAndWaitUntilStarted() needs to be called.
     * @return the added CloudStorageLauncher
     */
    CloudStorageLauncher& addServer();
    CloudStorageLauncher& server(int index);
    const CloudStorageLauncher& server(int index) const;

private:
    void addClusterArgs(CloudStorageLauncher* server);

private:
    const nx::utils::Url m_discoveryServiceUrl;
    std::vector<std::unique_ptr<CloudStorageLauncher>> m_servers;
};

} // namespace nx::cloud::storage::service::test