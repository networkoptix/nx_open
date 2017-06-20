#include "stats_manager.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace hpm {
namespace stats {

StatsManager::StatsManager(const conf::Settings& settings):
    m_instanceController(settings.dbConnectionOptions())
{
    m_collector = std::make_unique<Collector>(
        settings.statistics(),
        &m_instanceController.queryExecutor());
}

Collector& StatsManager::collector()
{
    return *m_collector;
}

const Collector& StatsManager::collector() const
{
    return *m_collector;
}

} // namespace stats
} // namespace hpm
} // namespace nx
