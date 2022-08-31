// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "http_statistics.h"
#include "http_message_dispatcher.h"

namespace nx::network::http::server {

/**
 * Takes and combines statistics from different sources.
 */
class NX_NETWORK_API AggregateStatisticsProvider:
    public nx::network::http::server::AbstractHttpStatisticsProvider
{
public:
    AggregateStatisticsProvider(
        const AbstractHttpStatisticsProvider& statsProvider,
        const AbstractMessageDispatcher& dispatcher);

    virtual HttpStatistics httpStatistics() const override;

private:
    const AbstractHttpStatisticsProvider& m_statsProvider;
    const AbstractMessageDispatcher& m_dispatcher;
};

} // namespace nx::network::http::server
