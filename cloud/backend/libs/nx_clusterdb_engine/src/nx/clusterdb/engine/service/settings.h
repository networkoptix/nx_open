#pragma once

#include <nx/network/http/server/settings.h>
#include <nx/sql/types.h>
#include <nx/utils/basic_service_settings.h>

#include "../p2p_sync_settings.h"

class QnSettings;

namespace nx::clusterdb::engine {

class NX_DATA_SYNC_ENGINE_API Settings:
    public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings();

    virtual QString dataDir() const override;
    virtual utils::log::Settings logging() const override;

    const SynchronizationSettings& synchronization() const;
    const nx::sql::ConnectionOptions& db() const;
    const nx::network::http::server::Settings& http() const;

protected:
    virtual void loadSettings() override;

private:
    utils::log::Settings m_logging;
    SynchronizationSettings m_syncSettings;
    nx::sql::ConnectionOptions m_db;
    nx::network::http::server::Settings m_http;
};

} // namespace nx::clusterdb::engine
