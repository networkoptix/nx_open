// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_statistics.h"

namespace nx {
namespace network {
namespace server {

void Statistics::add(const Statistics& right)
{
    // To sum up averages we need to calculate each value's weight first.
    const double thisWeight = connectionCount /
        double(std::max(connectionCount + right.connectionCount, 1));

    const double rightWeight = right.connectionCount /
        double(std::max(connectionCount + right.connectionCount, 1));

    connectionCount += right.connectionCount;
    connectionsAcceptedPerMinute += right.connectionsAcceptedPerMinute;
    requestsReceivedPerMinute += right.requestsReceivedPerMinute;

    requestsAveragePerConnection =
        thisWeight * requestsAveragePerConnection +
        rightWeight * right.requestsAveragePerConnection;
}

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
