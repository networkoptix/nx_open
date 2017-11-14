#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <nx/cloud/relaying/statistics.h>

namespace nx {
namespace cloud {

namespace relaying { class AbstractListeningPeerPool; }

namespace relay {
namespace controller {

struct Statistics
{
    relaying::Statistics relaying;
};

#define Statistics_relay_controller_Fields (relaying)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

class StatisticsProvider
{
public:
    StatisticsProvider(const relaying::AbstractListeningPeerPool& listeningPeerPool);
    virtual ~StatisticsProvider() = default;

    Statistics getAllStatistics() const;

private:
    const relaying::AbstractListeningPeerPool& m_listeningPeerPool;
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
