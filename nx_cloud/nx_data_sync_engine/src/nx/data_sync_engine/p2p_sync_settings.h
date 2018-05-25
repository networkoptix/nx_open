#pragma once

class QnSettings;

namespace nx {
namespace data_sync_engine {

class NX_DATA_SYNC_ENGINE_API Settings
{
public:
    unsigned int maxConcurrentConnectionsFromSystem;

    Settings();

    void load(const QnSettings& settings);
};

} // namespace data_sync_engine
} // namespace nx
