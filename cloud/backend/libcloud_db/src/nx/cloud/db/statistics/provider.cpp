#include "provider.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::db::statistics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _cdb_Fields)

//-------------------------------------------------------------------------------------------------

Provider::Provider(
    const network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
    const clusterdb::engine::statistics::Provider& dataSyncEngineStatisticsProvider,
    const nx::sql::StatisticsCollector& statisticsCollector)
    :
    m_httpServerStatisticsProvider(httpServerStatisticsProvider),
    m_dataSyncEngineStatisticsProvider(dataSyncEngineStatisticsProvider),
    m_statisticsCollector(statisticsCollector)
{
}

Statistics Provider::statistics() const
{
    return {m_httpServerStatisticsProvider.statistics(),
        m_dataSyncEngineStatisticsProvider.statistics(),
        m_statisticsCollector.getQueryStatistics()};
}

} // namespace nx::cloud::db::statistics
