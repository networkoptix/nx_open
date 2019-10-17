#include "resource_controller.h"

#include <nx/utils/log/log.h>

namespace nx::vms::utils::metrics {

api::metrics::ResourceGroupValues ResourceController::values(Scope requestScope, bool formatted) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    api::metrics::ResourceGroupValues groupValues;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (auto values = monitor->values(requestScope, formatted); !values.empty())
            groupValues[id] = std::move(values);
    }

    return groupValues;
}

api::metrics::ResourceGroupAlarms ResourceController::alarms(Scope requestScope) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    api::metrics::ResourceGroupAlarms groupAlarms;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (auto alarms = monitor->alarms(requestScope); !alarms.empty())
            groupAlarms[id] = std::move(alarms);
    }

    return groupAlarms;
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
