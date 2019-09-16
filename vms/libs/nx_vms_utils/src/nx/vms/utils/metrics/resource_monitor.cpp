#include "resource_monitor.h"

#include <nx/utils/log/log.h>

namespace nx::vms::utils::metrics {

ResourceMonitor::ResourceMonitor(
    std::unique_ptr<ResourceDescription> resource, ValueGroupMonitors monitors)
:
    m_resource(std::move(resource)),
    m_monitors(std::move(monitors))
{
}

api::metrics::ResourceValues ResourceMonitor::current(Scope requestScope) const
{
    api::metrics::ResourceValues values;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (auto v = monitor->current(requestScope); !v.empty())
            values[id] = std::move(v);
    }

    const auto countValues =
        [&]()
        {
            size_t count = 0;
            for (const auto& [id, value]: values) count += value.size();
            return count;
        };

    NX_VERBOSE(this, "Return %1 %2 values in %3 groups", countValues(), requestScope, values.size());
    return values;
}

std::vector<api::metrics::Alarm> ResourceMonitor::alarms(Scope requestScope) const
{
    std::vector<api::metrics::Alarm> allAlarms;
    for (const auto& [groupId, monitor]: m_monitors)
    {
        auto alarms = monitor->alarms(requestScope);
        for (auto& alarm: alarms)
        {
            alarm.parameter = groupId + "." + alarm.parameter;
            allAlarms.push_back(std::move(alarm));
        }
    }

    NX_VERBOSE(this, "Return %1 %2 alarms", allAlarms.size(), requestScope);
    return allAlarms;
}

void ResourceMonitor::setRules(const api::metrics::ResourceRules& rules)
{
    for (const auto& [id, monitor]: m_monitors)
    {
        if (const auto it = rules.find(id); it != rules.end())
            monitor->setRules(it->second.values);
    }

    // TODO: Assert on unprocessed rules?
}

QString ResourceMonitor::idForToStringFromPtr() const
{
    return lm("%1 %2").args(m_resource->scope, m_resource->id);
}

} // namespace nx::vms::utils::metrics
