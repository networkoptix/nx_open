// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_statistics_calculator.h"

namespace nx {
namespace network {
namespace server {
namespace detail {

StatisticsCalculator::StatisticsCalculator():
    m_connectionsPerMinuteCalculator(std::chrono::minutes(1)),
    m_requestsReceivedPerMinuteCalculator(std::chrono::minutes(1)),
    m_requestsAveragePerConnectionCalculator(std::chrono::minutes(1))
{
}

Statistics StatisticsCalculator::statistics(int aliveConnectionCount) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    Statistics result;
    result.connectionCount = aliveConnectionCount;
    result.connectionsAcceptedPerMinute =
        m_connectionsPerMinuteCalculator.getSumPerLastPeriod();
    result.requestsAveragePerConnection =
        m_requestsAveragePerConnectionCalculator.getAveragePerLastPeriod();
    result.requestsReceivedPerMinute =
        m_requestsReceivedPerMinuteCalculator.getSumPerLastPeriod();
    return result;
}

void StatisticsCalculator::messageReceived()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_requestsReceivedPerMinuteCalculator.add(1);
    m_requestsAveragePerConnectionCalculator.add(1);
}

void StatisticsCalculator::connectionAccepted()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_connectionsPerMinuteCalculator.add(1);
}

} // namespace detail
} // namespace server
} // namespace network
} // namespace nx
