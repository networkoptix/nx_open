#pragma once

#include <nx/data_sync_engine/p2p_sync_settings.h>

namespace nx::kvdb {

class NX_KEY_VALUE_DB_API Settings
{
public:
    nx::data_sync_engine::Settings dataSyncEngineSettings;

    void load(const QnSettings& settings);
};

} // namespace nx::kvdb
