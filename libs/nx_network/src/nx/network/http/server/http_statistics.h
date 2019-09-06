#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>
#include <nx/utils/math/average_per_period.h>
#include <nx/utils/math/max_per_period.h>

namespace nx::network::http::server {

struct NX_NETWORK_API RequestStatistics
{
    std::chrono::microseconds maxRequestProcessingTimeUsec =
        std::chrono::microseconds(0);
    std::chrono::microseconds averageRequestProcessingTimeUsec =
        std::chrono::microseconds(0);
};

struct NX_NETWORK_API HttpStatistics:
    public network::server::Statistics,
    public RequestStatistics
{
    void assign(const RequestStatistics& other);
};

#define HttpStatistics_Fields \
    (connectionCount)(connectionsAcceptedPerMinute)(requestsServedPerMinute) \
    (requestsAveragePerConnection)(maxRequestProcessingTimeUsec)(averageRequestProcessingTimeUsec)

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
// RequestStatisticsCalculator

class NX_NETWORK_API RequestStatisticsCalculator
{
public:
    RequestStatisticsCalculator();

    void add(const std::chrono::microseconds& duration);

    RequestStatistics requestStatistics() const;

private:
    // AveragePerPeriod does not compile when std::chrono::milliseconds is used directly
    nx::utils::math::AveragePerPeriod<std::chrono::microseconds::rep>
        m_averageRequestProcessingTime;
    nx::utils::math::MaxPerMinute<std::chrono::microseconds> m_maxRequestProcessingTime;
};

//-------------------------------------------------------------------------------------------------
// AggregateHttpStatisticsProvider

class NX_NETWORK_API AggregateHttpStatisticsProvider: public AbstractHttpStatisticsProvider
{
public:
    AggregateHttpStatisticsProvider(std::vector<const AbstractHttpStatisticsProvider*> providers);

    virtual HttpStatistics httpStatistics() const override;

private:
    std::vector<const AbstractHttpStatisticsProvider*> m_providers;
};

} // namespace nx::network::http::server
