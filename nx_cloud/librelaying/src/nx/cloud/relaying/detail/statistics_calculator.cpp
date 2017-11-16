#include "statistics_calculator.h"

namespace nx {
namespace cloud {
namespace relaying {
namespace detail {

StatisticsCalculator::StatisticsCalculator():
    m_sumPerPeriod(std::chrono::minutes(1))
{
}

Statistics StatisticsCalculator::calculateStatistics(int currentServerCount) const
{
    Statistics statistics;
    statistics.connectionsCount = m_connectionCount;
    statistics.listeningServerCount = currentServerCount;
    statistics.connectionsAveragePerServerCount =
        currentServerCount > 0
        ? m_connectionCount / currentServerCount
        : 0;
    statistics.connectionsAcceptedPerMinute = m_sumPerPeriod.getSumPerLastPeriod();
    return statistics;
}

void StatisticsCalculator::connectionAccepted()
{
    ++m_connectionCount;
    m_sumPerPeriod.add(1);
}

void StatisticsCalculator::connectionUsed()
{
    --m_connectionCount;
}

void StatisticsCalculator::connectionClosed()
{
    --m_connectionCount;
}

} // namespace detail
} // namespace relaying
} // namespace cloud
} // namespace nx
