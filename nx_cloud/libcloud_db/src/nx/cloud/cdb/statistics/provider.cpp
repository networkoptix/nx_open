#include "provider.h"

#include <nx/fusion/model_functions.h>

namespace nx::cdb::statistics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _cdb_Fields)

//-------------------------------------------------------------------------------------------------

Provider::Provider(
    const network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
    const data_sync_engine::statistics::Provider& dataSyncEngineStatisticsProvider)
    :
    m_httpServerStatisticsProvider(httpServerStatisticsProvider),
    m_dataSyncEngineStatisticsProvider(dataSyncEngineStatisticsProvider)
{
}

Statistics Provider::statistics() const
{
    Statistics statistics;
    statistics.http = m_httpServerStatisticsProvider.statistics();
    statistics.dataSync = m_dataSyncEngineStatisticsProvider.statistics();
    return statistics;
}

} // namespace nx::cdb::statistics
