// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_monitor.h"

#include <nx/utils/elapsed_timer.h>
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
    return valuesWithScope(requestScope, formatted).first;
}

std::pair<api::metrics::ResourceValues, Scope> ResourceMonitor::valuesWithScope(Scope requestScope,
    bool formatted,
    const nx::utils::DotNotationString& filter) const
{
    nx::utils::ElapsedTimer timer(nx::utils::ElapsedTimerState::started);
    auto actualScope = requestScope;
    api::metrics::ResourceValues values;
    size_t count = 0;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (!filter.accepts(id))
            continue;

        nx::utils::DotNotationString nested;
        if (auto it = filter.findWithWildcard(id); it != filter.end())
            nested = it.value();

        if (auto v = monitor->valuesWithScope(requestScope, formatted, std::move(nested)); !v.first.empty())
        {
            count += v.first.size();
            values[id] = std::move(v.first);

            if (v.second == Scope::local)
                actualScope = Scope::local;
        }
    }

    NX_VERBOSE(this, "Return %1 %2 values in %3 groups in %4",
        count, requestScope, values.size(), timer.elapsed());
    return {values, actualScope};
}

api::metrics::ResourceAlarms ResourceMonitor::alarms(Scope requestScope) const
{
    return alarmsWithScope(requestScope).first;
}

std::pair<api::metrics::ResourceAlarms, Scope> ResourceMonitor::alarmsWithScope(Scope requestScope,
    const nx::utils::DotNotationString& filter) const
{
    nx::utils::ElapsedTimer timer(nx::utils::ElapsedTimerState::started);
    auto actualScope = requestScope;
    api::metrics::ResourceAlarms resourceAlarms;
    size_t count = 0;
    for (const auto& [groupId, monitor]: m_monitors)
    {
        if (!filter.accepts(groupId))
            continue;

        nx::utils::DotNotationString nested;
        if (auto it = filter.findWithWildcard(groupId); it != filter.end())
            nested = it.value();

        if (auto alarms = monitor->alarmsWithScope(requestScope, std::move(nested)); !alarms.first.empty())
        {
            count += alarms.first.size();
            resourceAlarms[groupId] = std::move(alarms.first);

            if (alarms.second == Scope::local)
                actualScope = Scope::local;
        }
    }

    NX_VERBOSE(this, "Return %1 %2 alarmed values in %3", count, requestScope, timer.elapsed());
    return {resourceAlarms, actualScope};
}

void ResourceMonitor::setRules(const api::metrics::ResourceRules& rules)
{
    const auto skipOnMissing = m_resource->scope == Scope::site;
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
    return nx::format("%1 %2").args(m_resource->scope, m_resource->id);
}

} // namespace nx::vms::utils::metrics
