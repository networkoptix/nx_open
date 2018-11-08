#pragma once

#include <memory>

#include "collector.h"
#include "dao/rdb/instance_controller.h"
#include "../settings.h"
#include "../listening_peer_pool.h"
#include "../peer_registrator.h"

namespace nx {
namespace hpm {
namespace stats {

class StatsManager
{
public:
    StatsManager(
        const conf::Settings& settings,
        const ListeningPeerPool& listeningPeerPool,
        const PeerRegistrator& peerRegistrator);

    AbstractCollector& collector();
    const AbstractCollector& collector() const;

    CloudConnectStatistics cloudConnectStatistics() const;

private:
    std::unique_ptr<dao::rdb::InstanceController> m_instanceController;
    std::unique_ptr<AbstractCollector> m_collector;
    StatisticsCalculator* m_calculator = nullptr;
    const ListeningPeerPool& m_listeningPeerPool;
    const PeerRegistrator& m_peerRegistrator;
};

} // namespace stats
} // namespace hpm
} // namespace nx
