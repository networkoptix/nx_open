// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregate_statistics_provider.h"

namespace nx::network::http::server {

AggregateStatisticsProvider::AggregateStatisticsProvider(
    const AbstractHttpStatisticsProvider& statsProvider,
    const AbstractMessageDispatcher& dispatcher)
    :
    m_statsProvider(statsProvider),
    m_dispatcher(dispatcher)
{
}

HttpStatistics AggregateStatisticsProvider::httpStatistics() const
{
    auto stats = m_statsProvider.httpStatistics();
    stats.notFound404 = m_dispatcher.dispatchFailures();
    stats.requests = m_dispatcher.requestPathStatistics();
    return stats;
}

} // namespace nx::network::http::server
