#pragma once

#include <nx/clusterdb/engine/p2p_sync_settings.h>

namespace nx::clusterdb::map {

/**
 * Settings to be used by the clusterdb map.
 * Settings exist under group name "clusterDbMap".
 */
class NX_KEY_VALUE_DB_API Settings
{
public:
    nx::clusterdb::engine::SynchronizationSettings synchronizationSettings;

    std::string clusterId;

    void load(const QnSettings& settings);
};

} // namespace nx::clusterdb::map
