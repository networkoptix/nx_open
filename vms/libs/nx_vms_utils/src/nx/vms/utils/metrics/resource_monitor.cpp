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

api::metrics::ResourceValues ResourceMonitor::values(Scope requestScope, bool formatted) const
{
    api::metrics::ResourceValues values;
    size_t count = 0;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (auto v = monitor->values(requestScope, formatted); !v.empty())
        {
            count += v.size();
            values[id] = std::move(v);
        }
    }

    NX_VERBOSE(this, "Return %1 %2 values in %3 groups", count, requestScope, values.size());
    return values;
}

api::metrics::ResourceAlarms ResourceMonitor::alarms(Scope requestScope) const
{
    api::metrics::ResourceAlarms resourceAlarms;
    size_t count = 0;
    for (const auto& [groupId, monitor]: m_monitors)
    {
        if (auto alarms = monitor->alarms(requestScope); !alarms.empty())
        {
            count += alarms.size();
            resourceAlarms[groupId] = std::move(alarms);
        }
    }

    NX_VERBOSE(this, "Return %1 %2 alarmed values", count, requestScope);
    return resourceAlarms;
}

void ResourceMonitor::setRules(const api::metrics::ResourceRules& rules)
{
    const auto skipOnMissing = m_resource->scope == Scope::system;
    for (const auto& [id, groupRules]: rules.values)
    {
        if (const auto& it = m_monitors.find(id); it != m_monitors.end())
        {
            it->second->setRules(groupRules.values, skipOnMissing);
        }
        else
        {
            NX_DEBUG(this, "Skip group %1 in rules", id);
            NX_ASSERT(skipOnMissing, "%1 unexpected group %2", this, id);
        }
    }
}

QString ResourceMonitor::idForToStringFromPtr() const
{
    return lm("%1 %2").args(m_resource->scope, m_resource->id);
}

} // namespace nx::vms::utils::metrics
