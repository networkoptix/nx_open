#include "http_statistics.h"

#include <nx/fusion/model_functions.h>

namespace nx::network::http::server {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (HttpStatistics),
    (json),
    _Fields)

void HttpStatistics::assign(const RequestStatistics& other)
{
    averageRequestProcessingTimeUsec = other.averageRequestProcessingTimeUsec;
    maxRequestProcessingTimeUsec = other.maxRequestProcessingTimeUsec;
}

//-------------------------------------------------------------------------------------------------
// RequestStatisticsCalculator

RequestStatisticsCalculator::RequestStatisticsCalculator():
    m_averageRequestProcessingTime(std::chrono::minutes(1))
{
}

void RequestStatisticsCalculator::add(const std::chrono::microseconds& duration)
{
    m_averageRequestProcessingTime.add(duration.count());
    m_maxRequestProcessingTime.add(duration);
}

RequestStatistics RequestStatisticsCalculator::requestStatistics() const
{
    RequestStatistics stats;
    stats.averageRequestProcessingTimeUsec =
        std::chrono::microseconds(m_averageRequestProcessingTime.getAveragePerLastPeriod());
    stats.maxRequestProcessingTimeUsec = m_maxRequestProcessingTime.getMaxPerLastPeriod();
    return stats;
}

AggregateHttpStatisticsProvider::AggregateHttpStatisticsProvider(
    std::vector<const AbstractHttpStatisticsProvider*> providers)
    :
    m_providers(std::move(providers))
{
}

HttpStatistics AggregateHttpStatisticsProvider::httpStatistics() const
{
    int totalRequestAverages = 0;
    std::chrono::microseconds totalRequestAverageProcessingTime(0);
    std::chrono::microseconds maxRequestProcessingTimeUsec(0);
    HttpStatistics accumulatedStats;

    for(const auto& provider: m_providers)
    {
        ++totalRequestAverages;
        HttpStatistics httpStats = provider->httpStatistics();
        accumulatedStats.add(httpStats);
        totalRequestAverageProcessingTime += httpStats.averageRequestProcessingTimeUsec;
        maxRequestProcessingTimeUsec =
            std::max(maxRequestProcessingTimeUsec, httpStats.maxRequestProcessingTimeUsec);
    }

    accumulatedStats.maxRequestProcessingTimeUsec = maxRequestProcessingTimeUsec;
    if (totalRequestAverages > 0)
    {
        accumulatedStats.averageRequestProcessingTimeUsec =
            totalRequestAverageProcessingTime / totalRequestAverages;
    }

    return accumulatedStats;
}

} // namespace nx::network::http::server
