#pragma once

#include <chrono>

#include <nx/utils/math/average_per_period.h>
#include <nx/utils/math/sum_per_period.h>

#include "../server_statistics.h"

namespace nx {
namespace network {
namespace server {
namespace detail {

class NX_NETWORK_API StatisticsCalculator
{
public:
    StatisticsCalculator();
    virtual ~StatisticsCalculator() = default;

    Statistics statistics(int aliveConnectionCount) const;

    void saveConnectionStatistics(
        std::chrono::milliseconds lifeDuration,
        int requestsServed);

    void connectionAccepted();

private:
    nx::utils::math::SumPerPeriod<int> m_connectionsPerMinuteCalculator;
    nx::utils::math::SumPerPeriod<int> m_requestsServedPerMinuteCalculator;
    nx::utils::math::AveragePerPeriod<int> m_requestsAveragePerConnectionCalculator;
};

} // namespace detail
} // namespace server
} // namespace network
} // namespace nx
