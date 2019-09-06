#pragma once

#include <memory>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>
#include <nx/network/http/server/http_statistics.h>

#include "geo_ip_statistics.h"
#include "stats_manager.h"

namespace nx {
namespace hpm {
namespace stats {

class StatsManager;

struct Statistics
{
    network::http::server::HttpStatistics http;
    network::server::Statistics stun;
    CloudConnectStatistics cloudConnect;
    geo_ip::ListeningPeerStatistics listeningPeers;
};

#define Statistics_mediator_Fields (http)(stun)(cloudConnect)(listeningPeers)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

class Provider
{
public:
    Provider(
        const StatsManager& statsManager,
        const network::http::server::AbstractHttpStatisticsProvider& httpServerStatisticsProvider,
        const network::server::AbstractStatisticsProvider& stunServerStatisticsProvider,
        geo_ip::StatisticsProvider geoIpStatisticsProvider);

    Statistics getAllStatistics() const;

private:
    const StatsManager& m_statsManager;
    const network::http::server::AbstractHttpStatisticsProvider& m_httpServerStatisticsProvider;
    const network::server::AbstractStatisticsProvider& m_stunServerStatisticsProvider;
    geo_ip::StatisticsProvider m_geoIpStatisticsProvider;
};

} // namespace stats
} // namespace hpm
} // namespace nx
