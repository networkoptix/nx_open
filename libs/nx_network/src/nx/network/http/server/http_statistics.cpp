// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_statistics.h"

#include <nx/fusion/model_functions.h>

namespace nx::network::http::server {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RequestStatistics, (json), RequestStatistics_server_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RequestPathStatistics, (json), RequestPathStatistics_server_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(HttpStatistics, (json), HttpStatistics_server_Fields)

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

void RequestStatisticsCalculator::processedRequest(std::chrono::microseconds duration)
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

//-------------------------------------------------------------------------------------------------
// RequestPathStatisticsCalculator

void RequestPathStatisticsCalculator::processingRequest()
{
    m_requestsPerMinute.add(1);
}

void RequestPathStatisticsCalculator::processedRequest(std::chrono::microseconds duration)
{
    m_requestStatsCalculator.processedRequest(duration);
}

RequestPathStatistics RequestPathStatisticsCalculator::requestPathStatistics() const
{
    const auto stats = m_requestStatsCalculator.requestStatistics();
    return RequestPathStatistics{
        {stats.maxRequestProcessingTimeUsec, stats.averageRequestProcessingTimeUsec},
        m_requestsPerMinute.getSumPerLastPeriod()
    };
}

//-------------------------------------------------------------------------------------------------
// AggregateHttpStatisticsProvider

AggregateHttpStatisticsProvider::AggregateHttpStatisticsProvider(
    std::vector<const AbstractHttpStatisticsProvider*> providers)
    :
    m_providers(std::move(providers))
{
}

HttpStatistics AggregateHttpStatisticsProvider::httpStatistics() const
{
    std::chrono::microseconds requestAverageProcessingTimeSum(0);
    HttpStatistics accumulatedStats;

    // NOTE: declaring this map outside of providers loop because it is possible that multiple
    // providers have the statistics for the same path.
    std::map<
        std::string,
        std::pair<
            int /*count*/,
            std::chrono::microseconds /*averageRequestProcessingTimeUsecSum*/>
    > pathToAverageContext;

    for (const auto& provider: m_providers)
    {
        HttpStatistics httpStats = provider->httpStatistics();
        accumulatedStats.add(httpStats);
        requestAverageProcessingTimeSum += httpStats.averageRequestProcessingTimeUsec;
        accumulatedStats.maxRequestProcessingTimeUsec = std::max(
            accumulatedStats.maxRequestProcessingTimeUsec,
            httpStats.maxRequestProcessingTimeUsec);
        accumulatedStats.notFound404 += httpStats.notFound404;

        for (const auto& [path, stats]: httpStats.requests)
        {
            auto& accumulatedPathStats = accumulatedStats.requests[path];
            accumulatedPathStats.maxRequestProcessingTimeUsec = std::max(
                accumulatedPathStats.maxRequestProcessingTimeUsec,
                stats.maxRequestProcessingTimeUsec);
            accumulatedPathStats.requestsServedPerMinute += stats.requestsServedPerMinute;

            auto& averageContext = pathToAverageContext[path];
            ++averageContext.first;
            averageContext.second += stats.averageRequestProcessingTimeUsec;
        }
    }

    for (const auto& [path, averageContext]: pathToAverageContext)
    {
        if (averageContext.first)
        {
            accumulatedStats.requests[path].averageRequestProcessingTimeUsec =
                averageContext.second / averageContext.first;
        }
    }

    if (m_providers.size() > 0)
    {
        accumulatedStats.averageRequestProcessingTimeUsec =
            requestAverageProcessingTimeSum / m_providers.size();
    }

    return accumulatedStats;
}

} // namespace nx::network::http::server
