#pragma once

#include <nx/sql/types.h>
#include <nx/cloud/discovery/settings.h>
#include <nx/network/http/server/settings.h>
#include <nx/utils/basic_service_settings.h>

#include "../p2p_sync_settings.h"

class QnSettings;

namespace nx::clusterdb::engine {

class NX_DATA_SYNC_ENGINE_API Settings:
    public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings(
        const QString& organizationName = QString("nx"),
        const QString& applicationName = QString("clusterdb_engine_service"),
        const QString& moduleName = QString("clusterdb_engine"));

    virtual QString dataDir() const override;
    virtual utils::log::Settings logging() const override;

    const SynchronizationSettings& synchronization() const;
    const nx::sql::ConnectionOptions& db() const;
    const nx::network::http::server::Settings& http() const;
    const nx::cloud::discovery::Settings& discovery() const;

protected:
    virtual void loadSettings() override;

private:
    nx::utils::log::Settings m_logging;
    SynchronizationSettings m_syncSettings;
    nx::sql::ConnectionOptions m_db;
    nx::network::http::server::Settings m_http;
    nx::cloud::discovery::Settings m_discovery;
};

} // namespace nx::clusterdb::engine
