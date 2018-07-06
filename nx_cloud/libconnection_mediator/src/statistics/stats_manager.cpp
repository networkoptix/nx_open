#include "stats_manager.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace hpm {
namespace stats {

StatsManager::StatsManager(const conf::Settings& settings)
{
    if (settings.statistics().enabled)
    {
        NX_INFO(this, lm("Starting statistics collector"));

        m_instanceController = std::make_unique<dao::rdb::InstanceController>(
            settings.dbConnectionOptions());
        m_collector = CollectorFactory::instance().create(
            settings.statistics(),
            &m_instanceController->queryExecutor());
    }
    else
    {
        NX_INFO(this, lm("Starting without statistics collector"));

        m_collector = std::make_unique<DummyCollector>();
    }
}

AbstractCollector& StatsManager::collector()
{
    return *m_collector;
}

const AbstractCollector& StatsManager::collector() const
{
    return *m_collector;
}

} // namespace stats
} // namespace hpm
} // namespace nx
