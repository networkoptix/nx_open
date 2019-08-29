#include "resource_controller.h"

namespace nx::vms::utils::metrics {

api::metrics::ResourceGroupValues ResourceController::values() const
{
    api::metrics::ResourceGroupValues values;
    for (const auto& [id, monitor]: m_monitors)
        values[id] = monitor->current();

    return values;
}

std::vector<api::metrics::Alarm> ResourceController::alarms() const
{
    std::vector<api::metrics::Alarm> allAlarms;
    for (const auto& [id, monitor]: m_monitors)
    {
        auto alarms = monitor->alarms();
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
    NX_ASSERT(isOk, "Resource duplicate: %1", id);
}

void ResourceController::remove(const QString& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_monitors.erase(id); // TODO: Move destruction out of the mutex.
}

} // namespace nx::vms::utils::metrics
