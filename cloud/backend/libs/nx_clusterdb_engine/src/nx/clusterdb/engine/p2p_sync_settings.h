#pragma once

#include <string>

#include <nx/cloud/discovery/settings.h>

class QnSettings;

namespace nx::clusterdb::engine {

class NX_DATA_SYNC_ENGINE_API SynchronizationSettings
{
public:
    /** If empty, it is assigned to auto-generated guid. */
    std::string nodeId;
    unsigned int maxConcurrentConnectionsFromSystem;
    nx::cloud::discovery::Settings discovery;

    SynchronizationSettings();

    void load(const QnSettings& settings);
};

} // namespace nx::clusterdb::engine
