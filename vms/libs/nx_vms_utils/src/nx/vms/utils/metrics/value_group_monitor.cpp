#include "value_group_monitor.h"

#include <nx/utils/log/log.h>

#include "rule_monitors.h"

namespace nx::vms::utils::metrics {

ValueGroupMonitor::ValueGroupMonitor(ValueMonitors monitors):
    m_valueMonitors(std::move(monitors))
{
}

api::metrics::ValueGroup ValueGroupMonitor::current(Scope requiredScope) const
{
    // TODO: Use RW lock.
    NX_MUTEX_LOCKER locker(&m_mutex);

    api::metrics::ValueGroup values;
    for (const auto& [id, monitor]: m_valueMonitors)
    {
        if (requiredScope == Scope::local && monitor->scope() == Scope::system)
            continue;

        if (auto v = monitor->current(); !v.isNull())
            values[id] = std::move(v);
    }

    return values;
}

std::vector<api::metrics::Alarm> ValueGroupMonitor::alarms(Scope requiredScope) const
{
    std::vector<api::metrics::Alarm> alarms;
    for (const auto& monitor: m_alarmMonitors)
    {
        if (requiredScope == Scope::local && monitor->scope() == Scope::system)
            continue;

        if (auto alarm = monitor->currentAlarm())
            alarms.push_back(std::move(*alarm));
    }

    return alarms;
}

void ValueGroupMonitor::setRules(const std::map<QString, api::metrics::ValueRule>& rules)
{
    NX_MUTEX_LOCKER locker(&m_mutex);

    // TODO: Clean up m_valueMonitors.
    for (const auto& [parameterId, rule]: rules)
    {
        if (rule.calculate.isEmpty())
            continue;
        try
        {
            // TODO: All extra values should be emplaced before formula calculation. Currently some
            // arguments may appear missing during calculation, because they are supposed to added
            // later.
            m_valueMonitors.emplace(parameterId, std::make_unique<ExtraValueMonitor>(
                parseFormulaOrThrow(rule.calculate, m_valueMonitors)));
        }
        catch (const std::invalid_argument& error)
        {
            // TODO: This should be allowed for system resources only.
            NX_DEBUG(this, "Skip extra value %1: %2", parameterId, error.what());
        }
        catch (const std::logic_error& error)
        {
            NX_ASSERT(false, "Unable to add extra value %1: %2", parameterId, error.what());
        }
    }

    m_alarmMonitors.clear();
    for (const auto& [parameterId, rule]: rules)
    {
        for (const auto& alarmRule: rule.alarms)
        {
            try
            {
                m_alarmMonitors.push_back(std::make_unique<AlarmMonitor>(
                    parameterId,
                    alarmRule.level,
                    parseFormulaOrThrow(alarmRule.condition, m_valueMonitors),
                    parseTemplate(alarmRule.text, m_valueMonitors)
                ));
            }
            catch (const std::invalid_argument& error)
            {
                // TODO: This should be allowed for system resources only.
                NX_DEBUG(this, "Skip alarm monitor %1: %2", parameterId, error.what());
            }
            catch (const std::logic_error& error)
            {
                NX_ASSERT(false, "Unable to add alarm monitor %1: %2", parameterId, error.what());
            }
        }
    }
}

} // namespace nx::vms::utils::metrics
