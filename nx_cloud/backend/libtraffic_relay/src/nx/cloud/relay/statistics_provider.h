#pragma once

#include <memory>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/connection_server/server_statistics.h>
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
    nx::network::server::Statistics http;
    controller::RelaySessionStatistics relaySessions;

    bool operator==(const Statistics& right) const;
};

#define Statistics_relay_controller_Fields (relaying)(http)(relaySessions)

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
        const nx::network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
        const controller::AbstractTrafficRelay& trafficRelay);
    virtual ~StatisticsProvider() = default;

    virtual Statistics getAllStatistics() const override;

private:
    const relaying::AbstractListeningPeerPool& m_listeningPeerPool;
    const nx::network::server::AbstractStatisticsProvider& m_httpServerStatisticsProvider;
    const controller::AbstractTrafficRelay& m_trafficRelay;
};

//-------------------------------------------------------------------------------------------------

using StatisticsProviderFactoryFunc =
    std::unique_ptr<AbstractStatisticsProvider>(
        const relaying::AbstractListeningPeerPool&,
        const nx::network::server::AbstractStatisticsProvider&,
        const controller::AbstractTrafficRelay&);

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
        const nx::network::server::AbstractStatisticsProvider& httpServerStatisticsProvider,
        const controller::AbstractTrafficRelay& trafficRelay);
};

} // namespace relay
} // namespace cloud
} // namespace nx
