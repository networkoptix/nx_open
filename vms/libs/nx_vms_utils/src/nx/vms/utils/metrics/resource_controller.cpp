#include "resource_controller.h"

#include <nx/utils/log/log.h>

namespace nx::vms::utils::metrics {

api::metrics::ResourceGroupValues ResourceController::values(Scope requestScope) const
{
    api::metrics::ResourceGroupValues values;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (auto current = monitor->current(requestScope); !current.empty())
            values[id] = std::move(current);
    }

    return values;
}

std::vector<api::metrics::Alarm> ResourceController::alarms(Scope requestScope) const
{
    std::vector<api::metrics::Alarm> allAlarms;
    for (const auto& [id, monitor]: m_monitors)
    {
        auto alarms = monitor->alarms(requestScope);
        for (auto& alarm: alarms)
        {
            alarm.resource = id;
            allAlarms.push_back(std::move(alarm));
        }
    }

    return allAlarms;
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

void ResourceController::add(const QString& id, std::unique_ptr<ResourceMonitor> monitor)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    monitor->setRules(m_rules);

    auto [it, isOk] = m_monitors.emplace(id, std::move(monitor));
    if (NX_ASSERT(isOk, "Resource duplicate: %1", id))
        NX_INFO(this, "Add resource '%1' monitor: %2", id, it->second);

}

void ResourceController::remove(const QString& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto it = m_monitors.find(id);
    if (!NX_ASSERT(it != m_monitors.end(), "Unable to remove resource %1", id))
        return;

    NX_INFO(this, "Remove resource '%1' monitor: %2", id, it->second);
    m_monitors.erase(it); // TODO: Move destruction out of the mutex.
}

} // namespace nx::vms::utils::metrics
