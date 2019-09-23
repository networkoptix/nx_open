#include "cloud_storage_cluster.h"

namespace nx::cloud::storage::service::test {

namespace {

static constexpr char kClusterId[] = "nx_cloud_storage_test_cluster";

} // namespace

CloudStorageCluster::CloudStorageCluster(const nx::utils::Url& discoveryServiceUrl):
    m_discoveryServiceUrl(discoveryServiceUrl)
{
}

CloudStorageCluster::~CloudStorageCluster()
{
    for (auto& server : m_servers)
        server->stop();
}

CloudStorageLauncher& CloudStorageCluster::addServer()
{
    m_servers.emplace_back(std::make_unique<CloudStorageLauncher>());
    addClusterArgs(m_servers.back().get());
    return *m_servers.back();
}

CloudStorageLauncher& CloudStorageCluster::server(int index)
{
    return *m_servers[index];
}

const CloudStorageLauncher& CloudStorageCluster::server(int index) const
{
    return *m_servers[index];
}

void CloudStorageCluster::addClusterArgs(CloudStorageLauncher* server)
{
    std::string name = lm("cloud-storage-%1").arg(m_servers.size()).toStdString();
    server->addArg("-server/name", name.c_str());
    server->addArg("-database/p2pDb/nodeId", name.c_str());
    server->addArg("-database/p2pDb/clusterId", kClusterId);
    server->addArg("-database/p2pDb/discovery/enabled", "true");
    server->addArg(
        "-database/p2pDb/discovery/discoveryServiceUrl",
        m_discoveryServiceUrl.toStdString().c_str());
    server->addArg("-database/p2pDb/discovery/registrationErrorDelay", "1ms");
    server->addArg("-database/p2pDb/discovery/roundTripPadding", "1ms");
    server->addArg("-database/p2pDb/discovery/onlineNodesRequestDelay", "1s");
}

} // namespace nx::cloud::storage::service::test