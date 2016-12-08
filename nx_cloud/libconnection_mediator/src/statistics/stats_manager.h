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

    Collector& collector();
    const Collector& collector() const;

private:
    hpm::dao::rdb::InstanceController m_instanceController;
    std::unique_ptr<Collector> m_collector;
};

} // namespace stats
} // namespace hpm
} // namespace nx
