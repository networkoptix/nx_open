#include "stats_manager.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace hpm {
namespace stats {

StatsManager::StatsManager(
    const conf::Settings& settings,
    const ListeningPeerPool& listeningPeerPool,
    const PeerRegistrator& peerRegistrator)
    :
    m_listeningPeerPool(listeningPeerPool),
    m_peerRegistrator(peerRegistrator)
{
    std::vector<std::unique_ptr<AbstractCollector>> collectors;

    if (settings.statistics().enabled)
    {
        NX_INFO(this, lm("Starting persistent statistics collector"));

        m_instanceController = std::make_unique<dao::rdb::InstanceController>(
            settings.dbConnectionOptions());
        collectors.push_back(CollectorFactory::instance().create(
            settings.statistics(),
            &m_instanceController->queryExecutor()));
    }

    auto statisticsCalculator = std::make_unique<StatisticsCalculator>();
    m_calculator = statisticsCalculator.get();
    collectors.push_back(std::move(statisticsCalculator));

    m_collector = std::make_unique<StatisticsForwarder>(std::move(collectors));
}

AbstractCollector& StatsManager::collector()
{
    return *m_collector;
}

const AbstractCollector& StatsManager::collector() const
{
    return *m_collector;
}

CloudConnectStatistics StatsManager::cloudConnectStatistics() const
{
    auto statistics = m_calculator->statistics();
    statistics.serverCount = m_listeningPeerPool.listeningPeerCount();
    statistics.clientCount = m_peerRegistrator.boundClientCount();
    return statistics;
}

} // namespace stats
} // namespace hpm
} // namespace nx
