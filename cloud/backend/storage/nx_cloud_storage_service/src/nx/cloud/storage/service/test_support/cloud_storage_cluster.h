#pragma once

#include "cloud_storage_launcher.h"

namespace nx::cloud::storage::service::test {

class CloudStorageCluster
{
public:
    CloudStorageCluster(const nx::utils::Url& discoveryServiceUrl);
    ~CloudStorageCluster();

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