#pragma once

#include <memory>

#include <nx/clusterdb/engine/statistics/provider.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/server/http_statistics.h>
#include <nx/sql/db_statistics_collector.h>
#include <nx/utils/basic_factory.h>

#include <nx/cloud/relaying/statistics.h>

#include "controller/traffic_relay.h"

namespace nx {
namespace cloud {

namespace relaying { class AbstractListeningPeerPool; }

namespace relay {

struct Statistics
{
    relaying::Statistics relaying;
    nx::network::http::server::HttpStatistics http;
    controller::RelaySessionStatistics relaySessions;
    std::optional<nx::clusterdb::engine::statistics::Statistics> peerDb;
    std::optional<nx::sql::QueryStatistics> sql;
};

#define Statistics_relay_controller_Fields (relaying)(http)(relaySessions)(peerDb)(sql)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

//-------------------------------------------------------------------------------------------------

class AbstractStatisticsProvider
{
public:
    virtual ~AbstractStatisticsProvider() = default;

    virtual Statistics getAllStatistics() const = 0;
};

class StatisticsProvider:
    public AbstractStatisticsProvider
{
public:
    StatisticsProvider(
        const relaying::AbstractListeningPeerPool& listeningPeerPool,
        const network::http::server::AbstractHttpStatisticsProvider& httpServerStatisticsProvider,
        const controller::AbstractTrafficRelay& trafficRelay,
        const nx::clusterdb::engine::statistics::Provider* peerDbStatisticsProvider,
        const nx::sql::StatisticsCollector* sqlStatisticsProvider);
    virtual ~StatisticsProvider() = default;

    virtual Statistics getAllStatistics() const override;

private:
    const relaying::AbstractListeningPeerPool& m_listeningPeerPool;
    const network::http::server::AbstractHttpStatisticsProvider& m_httpServerStatisticsProvider;
    const controller::AbstractTrafficRelay& m_trafficRelay;
    const nx::clusterdb::engine::statistics::Provider* m_peerDbStatisticsProvider = nullptr;
    const nx::sql::StatisticsCollector* m_sqlStatisticsProvider = nullptr;
};

//-------------------------------------------------------------------------------------------------

using StatisticsProviderFactoryFunc =
    std::unique_ptr<AbstractStatisticsProvider>(
        const relaying::AbstractListeningPeerPool&,
        const nx::network::http::server::AbstractHttpStatisticsProvider&,
        const controller::AbstractTrafficRelay&,
        const nx::clusterdb::engine::statistics::Provider*,
        const nx::sql::StatisticsCollector*);

class StatisticsProviderFactory:
    public nx::utils::BasicFactory<StatisticsProviderFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<StatisticsProviderFactoryFunc>;

public:
    StatisticsProviderFactory();

    static StatisticsProviderFactory& instance();

private:
    std::unique_ptr<AbstractStatisticsProvider> defaultFactoryFunction(
        const relaying::AbstractListeningPeerPool& listeningPeerPool,
        const network::http::server::AbstractHttpStatisticsProvider& httpServerStatisticsProvider,
        const controller::AbstractTrafficRelay& trafficRelay,
        const nx::clusterdb::engine::statistics::Provider* peerDbStatisticsProvider,
        const nx::sql::StatisticsCollector* sqlStatistics);
};

} // namespace relay
} // namespace cloud
} // namespace nx
