#pragma once

#include <memory>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/basic_factory.h>

#include <nx/cloud/relaying/statistics.h>

namespace nx {
namespace cloud {

namespace relaying { class AbstractListeningPeerPool; }

namespace relay {
namespace controller {

struct Statistics
{
    relaying::Statistics relaying;

    bool operator==(const Statistics& right) const;
};

#define Statistics_relay_controller_Fields (relaying)

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
    StatisticsProvider(const relaying::AbstractListeningPeerPool& listeningPeerPool);
    virtual ~StatisticsProvider() = default;

    virtual Statistics getAllStatistics() const override;

private:
    const relaying::AbstractListeningPeerPool& m_listeningPeerPool;
};

//-------------------------------------------------------------------------------------------------

using StatisticsProviderFactoryFunc =
    std::unique_ptr<AbstractStatisticsProvider>(
        const relaying::AbstractListeningPeerPool& /*listeningPeerPool*/);

class StatisticsProviderFactory:
    public nx::utils::BasicFactory<StatisticsProviderFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<StatisticsProviderFactoryFunc>;

public:
    StatisticsProviderFactory();

    static StatisticsProviderFactory& instance();

private:
    std::unique_ptr<AbstractStatisticsProvider> defaultFactoryFunction(
        const relaying::AbstractListeningPeerPool& listeningPeerPool);
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
