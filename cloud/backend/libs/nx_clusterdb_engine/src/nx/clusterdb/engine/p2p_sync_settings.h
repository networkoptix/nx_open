#pragma once

#include <string>

class QnSettings;

namespace nx::clusterdb::engine {

class NX_DATA_SYNC_ENGINE_API SynchronizationSettings
{
public:
    /** If empty, it is assigned to auto-generated guid. */
    std::string nodeId;
    unsigned int maxConcurrentConnectionsFromSystem;

    SynchronizationSettings();

    void load(const QnSettings& settings);
};

} // namespace nx::clusterdb::engine
