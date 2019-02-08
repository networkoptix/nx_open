#pragma once

#include <nx/clusterdb/engine/statistics/provider.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>

namespace nx::cloud::db::statistics {

struct Statistics
{
    network::server::Statistics http;
    clusterdb::engine::statistics::Statistics dataSync;
};

#define Statistics_cdb_Fields (http)(dataSync)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

//-------------------------------------------------------------------------------------------------

class Provider
{
public:
    Provider(
        const network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
        const clusterdb::engine::statistics::Provider& dataSyncEngineStatisticsProvider);

    Statistics statistics() const;

private:
    const network::server::AbstractStatisticsProvider& m_httpServerStatisticsProvider;
    const clusterdb::engine::statistics::Provider& m_dataSyncEngineStatisticsProvider;
};

} // namespace nx::cloud::db::statistics
