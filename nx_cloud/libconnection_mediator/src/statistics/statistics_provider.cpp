#include "statistics_provider.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace hpm {
namespace stats {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _mediator_Fields)

//-------------------------------------------------------------------------------------------------

Provider::Provider(
    const StatsManager& statsManager,
    const nx::network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
    const nx::network::server::AbstractStatisticsProvider& stunServerStatisticsProvider)
    :
    m_statsManager(statsManager),
    m_httpServerStatisticsProvider(httpServerStatisticsProvider),
    m_stunServerStatisticsProvider(stunServerStatisticsProvider)
{
}

Statistics Provider::getAllStatistics() const
{
    Statistics statistics;
    statistics.http = m_httpServerStatisticsProvider.statistics();
    statistics.stun = m_stunServerStatisticsProvider.statistics();
    statistics.cloudConnect = m_statsManager.cloudConnectStatistics();
    return statistics;
}

} // namespace stats
} // namespace hpm
} // namespace nx
