#include "server_statistics.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace network {
namespace server {

void Statistics::add(const Statistics& right)
{
    connectionCount += right.connectionCount;
    connectionsAcceptedPerMinute += right.connectionsAcceptedPerMinute;
    requestsServedPerMinute += right.requestsServedPerMinute;
    requestsAveragePerConnection += right.requestsAveragePerConnection;
}

bool Statistics::operator==(const Statistics& right) const
{
    return connectionCount == right.connectionCount
        && connectionsAcceptedPerMinute == right.connectionsAcceptedPerMinute
        && requestsServedPerMinute == right.requestsServedPerMinute
        && requestsAveragePerConnection == right.requestsAveragePerConnection;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _server_Fields)

//-------------------------------------------------------------------------------------------------

AggregateStatisticsProvider::AggregateStatisticsProvider(
    const std::vector<const AbstractStatisticsProvider*>& providers)
    :
    m_providers(providers)
{
}

Statistics AggregateStatisticsProvider::statistics() const
{
    Statistics result;
    for (const auto& provider: m_providers)
        result.add(provider->statistics());

    return result;
}

} // namespace server
} // namespace network
} // namespace nx
