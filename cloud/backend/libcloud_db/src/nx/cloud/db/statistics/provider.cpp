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
    const clusterdb::engine::statistics::Provider& dataSyncEngineStatisticsProvider)
    :
    m_httpServerStatisticsProvider(httpServerStatisticsProvider),
    m_dataSyncEngineStatisticsProvider(dataSyncEngineStatisticsProvider)
{
}

Statistics Provider::statistics() const
{
    return {m_httpServerStatisticsProvider.statistics(),
        m_dataSyncEngineStatisticsProvider.statistics()};
}

} // namespace nx::cloud::db::statistics
