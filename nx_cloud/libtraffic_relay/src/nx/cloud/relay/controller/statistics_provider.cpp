#include "statistics_provider.h"

#include <nx/fusion/model_functions.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _relay_controller_Fields)

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
