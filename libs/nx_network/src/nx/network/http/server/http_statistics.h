// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/connection_server/server_statistics.h>
#include <nx/network/http/http_status.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/data_structures/partitioned_concurrent_hash_map.h>
#include <nx/utils/math/average_per_period.h>
#include <nx/utils/math/max_per_period.h>
#include <nx/utils/math/percentile_per_period.h>

namespace nx::network::http::server {

/**
 * Statistics per request method and URL path, e.g. GET /servers/{serverId}/client_session
 */
struct NX_NETWORK_API RequestStatistics
{
    std::chrono::microseconds maxRequestProcessingTimeUsec{0};
    std::chrono::microseconds averageRequestProcessingTimeUsec{0};
    std::map<int /* percentile */, std::chrono::microseconds> requestProcessingTimePercentilesUsec;
    int requestsServedPerMinute = 0;
    std::map<int /*HTTP status*/, int /*count*/> statuses;
};

#define RequestStatistics_server_Fields\
    (maxRequestProcessingTimeUsec)\
    (averageRequestProcessingTimeUsec)\
    (requestProcessingTimePercentilesUsec)\
    (requestsServedPerMinute)\
    (statuses)

NX_REFLECTION_INSTRUMENT(RequestStatistics, RequestStatistics_server_Fields)

//-------------------------------------------------------------------------------------------------

/**
 * General HTTP statistics reported by a server.
 * NOTE: Inheritance from network::server::Statistics and RequestStatistics are for aggregating
 * statistics for all HTTP requests.
 */
struct NX_NETWORK_API HttpStatistics:
    public network::server::Statistics,
    public RequestStatistics
{
    std::map<std::string /*requestLine*/, RequestStatistics> requests;

    using RequestStatistics::operator=;
};

#define HttpStatistics_server_Fields\
    Statistics_server_Fields\
    RequestStatistics_server_Fields\
    (requests)

NX_REFLECTION_INSTRUMENT(HttpStatistics, HttpStatistics_server_Fields)

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

/**
 * Calculates statistics per request i.e. one instance for one request line:
 * GET /servers/{serverId}/client_sessions
 */
class NX_NETWORK_API RequestStatisticsCalculator
{
public:
    RequestStatisticsCalculator();

    void processedRequest(std::chrono::microseconds duration, StatusCode::Value status);

    RequestStatistics statistics() const;

private:
    using PercentilePerPeriod = nx::utils::math::PercentilePerPeriod<std::chrono::microseconds>;

    // AveragePerPeriod does not compile when std::chrono::microseconds is used directly.
    nx::utils::math::AveragePerPeriod<std::chrono::microseconds::rep>
        m_averageRequestProcessingTime;

    nx::utils::math::MaxPerMinute<std::chrono::microseconds> m_maxRequestProcessingTime;
    std::map<int, PercentilePerPeriod> m_requestProcessingTimePercentiles;
    nx::utils::math::SumPerMinute<int> m_requestsServedPerMinute;
    std::map<StatusCode::Value, nx::utils::math::SumPerMinute<int>> m_statusesPerMinute;
};

//-------------------------------------------------------------------------------------------------
// SummingStatisticsProvider

/**
 * Sums up statistics from multiple sources.
 */
class NX_NETWORK_API SummingStatisticsProvider:
    public AbstractHttpStatisticsProvider
{
public:
    SummingStatisticsProvider(std::vector<const AbstractHttpStatisticsProvider*> providers);

    virtual HttpStatistics httpStatistics() const override;

private:
    std::vector<const AbstractHttpStatisticsProvider*> m_providers;
};

} // namespace nx::network::http::server
