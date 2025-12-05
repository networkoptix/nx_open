// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_statistics.h"

namespace nx::network::http::server {

//-------------------------------------------------------------------------------------------------
// RequestStatisticsCalculator

RequestStatisticsCalculator::RequestStatisticsCalculator():
    m_averageRequestProcessingTime(std::chrono::minutes(1))
{
    m_requestProcessingTimePercentiles.emplace(
        50, PercentilePerPeriod{0.5, std::chrono::minutes(1), 2});
    m_requestProcessingTimePercentiles.emplace(
        95, PercentilePerPeriod{0.95, std::chrono::minutes(1), 2});
    m_requestProcessingTimePercentiles.emplace(
        99, PercentilePerPeriod{0.99, std::chrono::minutes(1), 2});
}

void RequestStatisticsCalculator::processedRequest(
    std::chrono::microseconds duration,
    StatusCode::Value status)
{
    m_averageRequestProcessingTime.add(duration.count());
    m_maxRequestProcessingTime.add(duration);
    for (auto& p: m_requestProcessingTimePercentiles)
        p.second.add(duration);
    m_requestsServedPerMinute.add(1);
    m_statusesPerMinute[status].add(1);
}

RequestStatistics RequestStatisticsCalculator::statistics() const
{
    RequestStatistics stats;
    stats.averageRequestProcessingTimeUsec =
        std::chrono::microseconds(m_averageRequestProcessingTime.getAveragePerLastPeriod());
    stats.maxRequestProcessingTimeUsec = m_maxRequestProcessingTime.getMaxPerLastPeriod();

    for (const auto& [percentile, calculator]: m_requestProcessingTimePercentiles)
        stats.requestProcessingTimePercentilesUsec[percentile] = calculator.get();

    stats.requestsServedPerMinute = m_requestsServedPerMinute.getSumPerLastPeriod();

    for (const auto& [status, calculator]: m_statusesPerMinute)
    {
        if (const auto count = calculator.getSumPerLastPeriod(); count)
            stats.statuses[status] = count;
    }

    return stats;
}

//-------------------------------------------------------------------------------------------------
// SummingStatisticsProvider

SummingStatisticsProvider::SummingStatisticsProvider(
    std::vector<const AbstractHttpStatisticsProvider*> providers)
    :
    m_providers(std::move(providers))
{
}

HttpStatistics SummingStatisticsProvider::httpStatistics() const
{
    HttpStatistics total;

    for (const auto& provider: m_providers)
    {
        HttpStatistics httpStats = provider->httpStatistics();
        total.add(httpStats);

        total.maxRequestProcessingTimeUsec = std::max(
            total.maxRequestProcessingTimeUsec,
            httpStats.maxRequestProcessingTimeUsec);

        total.averageRequestProcessingTimeUsec = std::max(
            total.averageRequestProcessingTimeUsec,
            httpStats.averageRequestProcessingTimeUsec);

        for (const auto& [key, value]: httpStats.requestProcessingTimePercentilesUsec)
        {
            auto& percentile = total.requestProcessingTimePercentilesUsec[key];
            percentile = std::max(percentile, value);
        }

        total.requestsServedPerMinute += httpStats.requestsServedPerMinute;

        for (const auto& [status, count]: httpStats.statuses)
            total.statuses[status] += count;

        for (const auto& [path, stats]: httpStats.requests)
        {
            auto& totalPathStats = total.requests[path];
            totalPathStats.maxRequestProcessingTimeUsec = std::max(
                totalPathStats.maxRequestProcessingTimeUsec,
                stats.maxRequestProcessingTimeUsec);

            totalPathStats.averageRequestProcessingTimeUsec = std::max(
                totalPathStats.averageRequestProcessingTimeUsec,
                stats.averageRequestProcessingTimeUsec);

            totalPathStats.requestsServedPerMinute += stats.requestsServedPerMinute;

            for (const auto& [status, count]: stats.statuses)
                totalPathStats.statuses[status] += count;
        }
    }

    return total;
}

} // namespace nx::network::http::server
