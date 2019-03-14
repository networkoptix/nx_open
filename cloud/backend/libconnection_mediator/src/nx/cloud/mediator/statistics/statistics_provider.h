#pragma once

#include <memory>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>

#include "stats_manager.h"

namespace nx {
namespace hpm {
namespace stats {

class StatsManager;

struct Statistics
{
    nx::network::server::Statistics http;
    nx::network::server::Statistics stun;
    CloudConnectStatistics cloudConnect;
};

#define Statistics_mediator_Fields (http)(stun)(cloudConnect)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

class Provider
{
public:
    Provider(
        const StatsManager& statsManager,
        const nx::network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
        const nx::network::server::AbstractStatisticsProvider& stunServerStatisticsProvider);

    Statistics getAllStatistics() const;

private:
    const StatsManager& m_statsManager;
    const nx::network::server::AbstractStatisticsProvider& m_httpServerStatisticsProvider;
    const nx::network::server::AbstractStatisticsProvider& m_stunServerStatisticsProvider;
};

} // namespace stats
} // namespace hpm
} // namespace nx
