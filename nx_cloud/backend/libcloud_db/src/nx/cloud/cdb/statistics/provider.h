#pragma once

#include <nx/data_sync_engine/statistics/provider.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>

namespace nx::cdb::statistics {

struct Statistics
{
    network::server::Statistics http;
    data_sync_engine::statistics::Statistics dataSync;
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
        const data_sync_engine::statistics::Provider& dataSyncEngineStatisticsProvider);

    Statistics statistics() const;

private:
    const network::server::AbstractStatisticsProvider& m_httpServerStatisticsProvider;
    const data_sync_engine::statistics::Provider& m_dataSyncEngineStatisticsProvider;
};

} // namespace nx::cdb::statistics
