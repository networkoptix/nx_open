// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_controller.h"

#include <nx/utils/log/log.h>

namespace nx::vms::utils::metrics {

api::metrics::ResourceGroupValues ResourceController::values(Scope requestScope, bool formatted)
{
    return valuesWithScope(requestScope, formatted).first;
}

std::pair<api::metrics::ResourceGroupValues, Scope> ResourceController::valuesWithScope(Scope requestScope,
    bool formatted,
    const nx::utils::DotNotationString& filter)
{
    beforeValues(requestScope, formatted);

    NX_MUTEX_LOCKER lock(&m_mutex);
    auto actualScope = requestScope;
    api::metrics::ResourceGroupValues groupValues;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (!filter.accepts(id))
            continue;

        nx::utils::DotNotationString nested;
        if (auto it = filter.findWithWildcard(id); it != filter.end())
            nested = it.value();

        if (auto values = monitor->valuesWithScope(requestScope, formatted, std::move(nested)); !values.first.empty())
        {
            groupValues[id] = std::move(values.first);

            if (values.second == Scope::local)
                actualScope = Scope::local;
        }
    }

    return {groupValues, actualScope};
}

api::metrics::ResourceGroupAlarms ResourceController::alarms(Scope requestScope)
{
    return alarmsWithScope(requestScope).first;
}

std::pair<api::metrics::ResourceGroupAlarms, Scope> ResourceController::alarmsWithScope(Scope requestScope,
    const nx::utils::DotNotationString& filter)
{
    beforeAlarms(requestScope);

    NX_MUTEX_LOCKER lock(&m_mutex);
    auto actualScope = requestScope;
    api::metrics::ResourceGroupAlarms groupAlarms;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (!filter.accepts(id))
            continue;

        nx::utils::DotNotationString nested;
        if (auto it = filter.findWithWildcard(id); it != filter.end())
            nested = it.value();

        if (auto alarms = monitor->alarmsWithScope(requestScope, std::move(nested)); !alarms.first.empty())
        {
            groupAlarms[id] = std::move(alarms.first);

            if (alarms.second == Scope::local)
                actualScope = Scope::local;
        }
    }

    return {groupAlarms, actualScope};
}

api::metrics::ResourceRules ResourceController::rules() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_rules;
}

void ResourceController::setRules(api::metrics::ResourceRules rules)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_rules = std::move(rules);

    for (const auto& [id, monitor]: m_monitors)
        monitor->setRules(m_rules);
}

void ResourceController::add(std::unique_ptr<ResourceMonitor> monitor)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    monitor->setRules(m_rules);

    auto& storedMonitor = m_monitors[monitor->id()];
    if (storedMonitor)
        NX_INFO(this, "Replace %1 with %2", storedMonitor, monitor);
    else
        NX_INFO(this, "Add %1", monitor);

    storedMonitor = std::move(monitor);
}

bool ResourceController::remove(const QString& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto it = m_monitors.find(id);
    if (it == m_monitors.end())
    {
        NX_WARNING(this, "Skip missing resource %1", id); // TODO: Debug instead?
        return false;
    }

    NX_INFO(this, "Remove %1", it->second);
    m_monitors.erase(it); // TODO: Move destruction out of the mutex.
    return true;
}

} // namespace nx::vms::utils::metrics
