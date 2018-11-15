#pragma once

class QnSettings;

namespace nx::clusterdb::engine {

class NX_DATA_SYNC_ENGINE_API SynchronizationSettings
{
public:
    unsigned int maxConcurrentConnectionsFromSystem;

    SynchronizationSettings();

    void load(const QnSettings& settings);
};

} // namespace nx::clusterdb::engine
