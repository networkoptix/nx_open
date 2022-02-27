// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
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

#define RequestStatistics_server_Fields\
    (maxRequestProcessingTimeUsec)(averageRequestProcessingTimeUsec)

QN_FUSION_DECLARE_FUNCTIONS(RequestStatistics, (json), NX_NETWORK_API)

NX_REFLECTION_INSTRUMENT(RequestStatistics, RequestStatistics_server_Fields)

//-------------------------------------------------------------------------------------------------

struct NX_NETWORK_API RequestPathStatistics: public RequestStatistics
{
    int requestsServedPerMinute = 0;
};

#define RequestPathStatistics_server_Fields\
    RequestStatistics_server_Fields\
    (requestsServedPerMinute)

QN_FUSION_DECLARE_FUNCTIONS(RequestPathStatistics, (json), NX_NETWORK_API)

NX_REFLECTION_INSTRUMENT(RequestPathStatistics, RequestPathStatistics_server_Fields)

//-------------------------------------------------------------------------------------------------

struct NX_NETWORK_API HttpStatistics:
    public network::server::Statistics,
    public RequestStatistics
{
    // Inherited fields are aggregate statistics

    void assign(const RequestStatistics& other);

    int notFound404 = 0;
    std::map<std::string/*requestPathTemplate*/, RequestPathStatistics> requests;
};

#define HttpStatistics_server_Fields\
    Statistics_server_Fields\
    RequestStatistics_server_Fields\
    (notFound404)(requests)

QN_FUSION_DECLARE_FUNCTIONS(HttpStatistics, (json), NX_NETWORK_API)

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

class NX_NETWORK_API RequestStatisticsCalculator
{
public:
    RequestStatisticsCalculator();

    void processedRequest(std::chrono::microseconds duration);

    RequestStatistics requestStatistics() const;

private:
    // AveragePerPeriod does not compile when std::chrono::milliseconds is used directly
    nx::utils::math::AveragePerPeriod<std::chrono::microseconds::rep>
        m_averageRequestProcessingTime;
    nx::utils::math::MaxPerMinute<std::chrono::microseconds> m_maxRequestProcessingTime;
};

//-------------------------------------------------------------------------------------------------
// RequestPathStatsCalculator

class NX_NETWORK_API RequestPathStatisticsCalculator
{
public:
    void processingRequest();
    void processedRequest(std::chrono::microseconds duration);

    RequestPathStatistics requestPathStatistics() const;

private:
    RequestStatisticsCalculator m_requestStatsCalculator;
    nx::utils::math::SumPerMinute<int> m_requestsPerMinute;
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
