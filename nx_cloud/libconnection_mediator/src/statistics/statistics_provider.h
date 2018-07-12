#pragma once

#include <memory>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>

namespace nx {
namespace hpm {
namespace stats {

struct Statistics
{
    nx::network::server::Statistics http;
    nx::network::server::Statistics stun;
};

#define Statistics_mediator_Fields (http)(stun)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

class Provider
{
public:
    Provider(
        const nx::network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
        const nx::network::server::AbstractStatisticsProvider& stunServerStatisticsProvider);

    Statistics getAllStatistics() const;

private:
    const nx::network::server::AbstractStatisticsProvider& m_httpServerStatisticsProvider;
    const nx::network::server::AbstractStatisticsProvider& m_stunServerStatisticsProvider;
};

} // namespace stats
} // namespace hpm
} // namespace nx
