#pragma once

class QnSettings;

namespace nx {
namespace data_sync_engine {

class NX_DATA_SYNC_ENGINE_API SynchronizationSettings
{
public:
    unsigned int maxConcurrentConnectionsFromSystem;

    SynchronizationSettings();

    void load(const QnSettings& settings);
};

} // namespace data_sync_engine
} // namespace nx
