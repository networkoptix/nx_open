#pragma once

#include <string>

#include <nx/cloud/discovery/settings.h>

class QnSettings;

namespace nx::clusterdb::engine {

/**
 * Settings used to configure SynchronizationEngine.
 * Settings exist under group name "p2pDb".
 */
class NX_DATA_SYNC_ENGINE_API SynchronizationSettings
{
public:
    static constexpr char kDefaultGroupName[] = "p2pDb";

    std::string clusterId;
    /** If empty, it is assigned to auto-generated guid. */
    std::string nodeId;
    unsigned int maxConcurrentConnectionsFromCluster;
    std::chrono::milliseconds nodeConnectRetryTimeout;
    nx::cloud::discovery::Settings discovery;
    bool groupCommandsUnderDbTransaction = false;

    SynchronizationSettings();

    void load(const QnSettings& settings, std::string groupId = kDefaultGroupName);
};

} // namespace nx::clusterdb::engine
