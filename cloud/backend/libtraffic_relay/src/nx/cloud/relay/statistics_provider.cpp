#include "statistics_provider.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _relay_controller_Fields)

//-------------------------------------------------------------------------------------------------

StatisticsProvider::StatisticsProvider(
    const relaying::AbstractListeningPeerPool& listeningPeerPool,
    const network::http::server::AbstractHttpStatisticsProvider& httpServerStatisticsProvider,
    const controller::AbstractTrafficRelay& trafficRelay,
    const nx::clusterdb::engine::statistics::Provider* peerDbStatisticsProvider,
    const nx::sql::StatisticsCollector* sqlStatisticsProvider)
    :
    m_listeningPeerPool(listeningPeerPool),
    m_httpServerStatisticsProvider(httpServerStatisticsProvider),
    m_trafficRelay(trafficRelay),
    m_peerDbStatisticsProvider(peerDbStatisticsProvider),
    m_sqlStatisticsProvider(sqlStatisticsProvider)
{
}

Statistics StatisticsProvider::getAllStatistics() const
{
    Statistics statistics;
    statistics.relaying = m_listeningPeerPool.statistics();
    statistics.http = m_httpServerStatisticsProvider.httpStatistics();
    statistics.relaySessions = m_trafficRelay.statistics();
    if (m_peerDbStatisticsProvider)
        statistics.peerDb = m_peerDbStatisticsProvider->statistics();
    if (m_sqlStatisticsProvider)
        statistics.sql = m_sqlStatisticsProvider->getQueryStatistics();
    return statistics;
}

//-------------------------------------------------------------------------------------------------

StatisticsProviderFactory::StatisticsProviderFactory():
    base_type(
        [this](auto&&... args)
        {
            return defaultFactoryFunction(std::forward<decltype(args)>(args)...);
        })
{
}

StatisticsProviderFactory& StatisticsProviderFactory::instance()
{
    static StatisticsProviderFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractStatisticsProvider> StatisticsProviderFactory::defaultFactoryFunction(
    const relaying::AbstractListeningPeerPool& listeningPeerPool,
    const nx::network::http::server::AbstractHttpStatisticsProvider& httpServerStatisticsProvider,
    const controller::AbstractTrafficRelay& trafficRelay,
    const nx::clusterdb::engine::statistics::Provider* peerDbStatisticsProvider,
    const nx::sql::StatisticsCollector* sqlStatistics)
{
    return std::make_unique<StatisticsProvider>(
        listeningPeerPool,
        httpServerStatisticsProvider,
        trafficRelay,
        peerDbStatisticsProvider,
        sqlStatistics);
}

} // namespace relay
} // namespace cloud
} // namespace nx
