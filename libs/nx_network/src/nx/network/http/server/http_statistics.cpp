#include "http_statistics.h"

#include <nx/fusion/model_functions.h>

namespace nx::network::http::server {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (HttpStatistics),
    (json),
    _Fields)

void HttpStatistics::assign(const RequestStatistics& other)
{
    averageRequestProcessingTime = other.averageRequestProcessingTime;
    maxRequestProcessingTime = other.maxRequestProcessingTime;
}

//-------------------------------------------------------------------------------------------------
// HttpStatisticsCalculator

HttpStatisticsCalculator::HttpStatisticsCalculator():
    m_averageRequestProcessingTime(std::chrono::minutes(1))
{
}

void HttpStatisticsCalculator::add(const std::chrono::milliseconds& duration)
{
    m_averageRequestProcessingTime.add(duration.count());
    m_maxRequestProcessingTime.add(duration);
}

RequestStatistics HttpStatisticsCalculator::requestStatistics() const
{
    RequestStatistics stats;
    stats.averageRequestProcessingTime =
        std::chrono::milliseconds(m_averageRequestProcessingTime.getAveragePerLastPeriod());
    stats.maxRequestProcessingTime = m_maxRequestProcessingTime.getMaxPerLastPeriod();
    return stats;
}

} // namespace nx::network::http::server
