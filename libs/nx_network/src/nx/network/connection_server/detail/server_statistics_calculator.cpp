#include "server_statistics_calculator.h"

namespace nx {
namespace network {
namespace server {
namespace detail {

StatisticsCalculator::StatisticsCalculator():
    m_connectionsPerMinuteCalculator(std::chrono::minutes(1)),
    m_requestsServedPerMinuteCalculator(std::chrono::minutes(1)),
    m_requestsAveragePerConnectionCalculator(std::chrono::minutes(1))
{
}

Statistics StatisticsCalculator::statistics(int aliveConnectionCount) const
{
    QnMutexLocker lock(&m_mutex);

    Statistics result;
    result.connectionCount = aliveConnectionCount;
    result.connectionsAcceptedPerMinute =
        m_connectionsPerMinuteCalculator.getSumPerLastPeriod();
    result.requestsAveragePerConnection =
        m_requestsAveragePerConnectionCalculator.getAveragePerLastPeriod();
    result.requestsServedPerMinute =
        m_requestsServedPerMinuteCalculator.getSumPerLastPeriod();
    return result;
}

void StatisticsCalculator::saveConnectionStatistics(
    std::chrono::milliseconds lifeDuration,
    int requestsServed)
{
    QnMutexLocker lock(&m_mutex);

    m_requestsServedPerMinuteCalculator.add(
        requestsServed, lifeDuration);
    m_requestsAveragePerConnectionCalculator.add(requestsServed);
}

void StatisticsCalculator::connectionAccepted()
{
    QnMutexLocker lock(&m_mutex);
    m_connectionsPerMinuteCalculator.add(1);
}

} // namespace detail
} // namespace server
} // namespace network
} // namespace nx
