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
    std::string clusterId;
    /** If empty, it is assigned to auto-generated guid. */
    std::string nodeId;
    unsigned int maxConcurrentConnectionsFromSystem;
    std::chrono::milliseconds nodeConnectRetryTimeout;
    nx::cloud::discovery::Settings discovery;

    SynchronizationSettings();

    void load(const QnSettings& settings, std::string groupId = "p2pDb");
};

} // namespace nx::clusterdb::engine
