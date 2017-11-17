#pragma once

#include <memory>

#include "collector.h"
#include "dao/rdb/instance_controller.h"
#include "settings.h"

namespace nx {
namespace hpm {
namespace stats {

class StatsManager
{
public:
    StatsManager(const conf::Settings& settings);

    AbstractCollector& collector();
    const AbstractCollector& collector() const;

private:
    std::unique_ptr<dao::rdb::InstanceController> m_instanceController;
    std::unique_ptr<AbstractCollector> m_collector;
};

} // namespace stats
} // namespace hpm
} // namespace nx
