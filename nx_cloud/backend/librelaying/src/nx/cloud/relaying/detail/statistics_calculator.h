#pragma once

#include <nx/utils/math/sum_per_period.h>

#include "../statistics.h"

namespace nx {
namespace cloud {
namespace relaying {
namespace detail {

/**
 * NOTE: Not thread-safe.
 */
class NX_RELAYING_API StatisticsCalculator
{
public:
    StatisticsCalculator();
    virtual ~StatisticsCalculator() = default;

    Statistics calculateStatistics(int serverCount) const;

    void connectionAccepted();
    void connectionUsed();
    void connectionClosed();

private:
    int m_connectionCount = 0;
    nx::utils::math::SumPerPeriod<int> m_sumPerPeriod;
};

} // namespace detail
} // namespace relaying
} // namespace cloud
} // namespace nx
