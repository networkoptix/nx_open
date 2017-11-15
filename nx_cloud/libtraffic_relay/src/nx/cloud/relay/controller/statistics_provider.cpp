#include "statistics_provider.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

bool Statistics::operator==(const Statistics& right) const
{
    return relaying == right.relaying;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _relay_controller_Fields)

//-------------------------------------------------------------------------------------------------

StatisticsProvider::StatisticsProvider(
    const relaying::AbstractListeningPeerPool& listeningPeerPool)
    :
    m_listeningPeerPool(listeningPeerPool)
{
}

Statistics StatisticsProvider::getAllStatistics() const
{
    Statistics statistics;
    statistics.relaying = m_listeningPeerPool.statistics();
    return statistics;
}

//-------------------------------------------------------------------------------------------------

StatisticsProviderFactory::StatisticsProviderFactory():
    base_type(std::bind(&StatisticsProviderFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
}

StatisticsProviderFactory& StatisticsProviderFactory::instance()
{
    static StatisticsProviderFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractStatisticsProvider> StatisticsProviderFactory::defaultFactoryFunction(
    const relaying::AbstractListeningPeerPool& listeningPeerPool)
{
    return std::make_unique<StatisticsProvider>(listeningPeerPool);
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
