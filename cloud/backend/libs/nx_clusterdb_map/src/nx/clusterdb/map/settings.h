#pragma once

#include <nx/clusterdb/engine/p2p_sync_settings.h>

namespace nx::clusterdb::map {

class NX_KEY_VALUE_DB_API Settings
{
public:
    nx::clusterdb::engine::SynchronizationSettings dataSyncEngineSettings;

    void load(const QnSettings& settings);
};

} // namespace nx::clusterdb::map
