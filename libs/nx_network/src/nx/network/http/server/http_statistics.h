#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>
#include <nx/utils/math/average_per_period.h>
#include <nx/utils/math/max_per_period.h>

namespace nx::network::http::server {

struct NX_NETWORK_API RequestStatistics
{
    std::chrono::milliseconds maxRequestProcessingTime =
        std::chrono::milliseconds(0);
    std::chrono::milliseconds averageRequestProcessingTime =
        std::chrono::milliseconds(0);
};

struct NX_NETWORK_API HttpStatistics:
    public network::server::Statistics,
    public RequestStatistics
{
    void assign(const RequestStatistics& other);
};

#define HttpStatistics_Fields \
    (connectionCount)(connectionsAcceptedPerMinute)(requestsServedPerMinute) \
    (requestsAveragePerConnection)(maxRequestProcessingTime)(averageRequestProcessingTime)

QN_FUSION_DECLARE_FUNCTIONS(HttpStatistics, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------
// AbstractHttpStatisticsProvider

class NX_NETWORK_API AbstractHttpStatisticsProvider
{
public:
    virtual ~AbstractHttpStatisticsProvider() = default;

    virtual HttpStatistics httpStatistics() const = 0;
};

//-------------------------------------------------------------------------------------------------
// HttpStatisticsCalculator

class NX_NETWORK_API HttpStatisticsCalculator
{
public:
    HttpStatisticsCalculator();

    void add(const std::chrono::milliseconds& duration);

    RequestStatistics requestStatistics() const;

private:
    // AveragePerPeriod does not compile when std::chrono::milliseconds is used directly
    nx::utils::math::AveragePerPeriod<std::chrono::milliseconds::rep>
        m_averageRequestProcessingTime;
    nx::utils::math::MaxPerMinute<std::chrono::milliseconds> m_maxRequestProcessingTime;
};

} // namespace nx::network::http::server
