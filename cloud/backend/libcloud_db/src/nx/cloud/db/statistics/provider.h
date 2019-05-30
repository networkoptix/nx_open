#pragma once

#include <nx/clusterdb/engine/statistics/provider.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>
#include <nx/sql/db_statistics_collector.h>

namespace nx::cloud::db::statistics {

struct Statistics
{
    network::server::Statistics http;
    clusterdb::engine::statistics::Statistics dataSync;
    nx::sql::QueryStatistics sql;
};

#define Statistics_cdb_Fields (http)(dataSync)(sql)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

//-------------------------------------------------------------------------------------------------

class Provider
{
public:
    Provider(
        const network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
        const clusterdb::engine::statistics::Provider& dataSyncEngineStatisticsProvider,
        const nx::sql::StatisticsCollector& statisticsCollector);

    Statistics statistics() const;

private:
    const network::server::AbstractStatisticsProvider& m_httpServerStatisticsProvider;
    const clusterdb::engine::statistics::Provider& m_dataSyncEngineStatisticsProvider;
    const nx::sql::StatisticsCollector& m_statisticsCollector;
};

} // namespace nx::cloud::db::statistics
