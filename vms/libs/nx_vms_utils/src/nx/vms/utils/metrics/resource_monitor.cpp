#include "resource_monitor.h"

namespace nx::vms::utils::metrics {

ResourceMonitor::ResourceMonitor(std::unique_ptr<Description> resource, ValueGroupMonitors monitors):
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

} // namespace nx::vms::utils::metrics
