// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <compare>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx {
namespace network {
namespace server {

struct NX_NETWORK_API Statistics
{
    int connectionCount = 0;
    int connectionsAcceptedPerMinute = 0;
    int requestsReceivedPerMinute = 0;
    /**
     * Calculated for connections closed in the last minute.
     */
    int requestsAveragePerConnection = 0;

    /**
     * Sums up *this and right statistics.
     */
    void add(const Statistics& right);

    auto operator<=>(const Statistics&) const = default;
};

#define Statistics_server_Fields (connectionCount)(connectionsAcceptedPerMinute) \
    (requestsReceivedPerMinute)(requestsAveragePerConnection)

NX_REFLECTION_INSTRUMENT(Statistics, Statistics_server_Fields)

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API AbstractStatisticsProvider
{
public:
    virtual ~AbstractStatisticsProvider() = default;

    virtual Statistics statistics() const = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API AggregateStatisticsProvider:
    public AbstractStatisticsProvider
{
public:
    AggregateStatisticsProvider(const std::vector<const AbstractStatisticsProvider*>& providers);

    virtual Statistics statistics() const;

private:
    const std::vector<const AbstractStatisticsProvider*> m_providers;
};

} // namespace server
} // namespace network
} // namespace nx
