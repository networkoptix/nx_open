// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/math/average_per_period.h>
#include <nx/utils/math/sum_per_period.h>
#include <nx/utils/thread/mutex.h>

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

    void messageReceived();

    void connectionAccepted();

private:
    nx::utils::math::SumPerPeriod<int> m_connectionsPerMinuteCalculator;
    nx::utils::math::SumPerPeriod<int> m_requestsReceivedPerMinuteCalculator;
    nx::utils::math::AveragePerPeriod<int> m_requestsAveragePerConnectionCalculator;
    mutable nx::Mutex m_mutex;
};

} // namespace detail
} // namespace server
} // namespace network
} // namespace nx
