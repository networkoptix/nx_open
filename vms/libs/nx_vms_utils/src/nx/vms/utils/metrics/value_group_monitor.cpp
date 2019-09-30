#include "value_group_monitor.h"

#include "rule_monitors.h"

namespace nx::vms::utils::metrics {

ValueGroupMonitor::ValueGroupMonitor(ValueMonitors monitors):
    m_valueMonitors(std::move(monitors))
{
}

api::metrics::ValueGroup ValueGroupMonitor::current() const
{
    // TODO: Use RW lock.
    NX_MUTEX_LOCKER locker(&m_mutex);

    api::metrics::ValueGroup values;
    for (const auto& [id, monitor]: m_valueMonitors)
    {
        if (auto v = monitor->current(); !v.isNull())
            values[id] = std::move(v);
    }

    return values;
}

std::vector<api::metrics::Alarm> ValueGroupMonitor::alarms() const
{
    std::vector<api::metrics::Alarm> alarms;
    for (const auto& monitor: m_alarmMonitors)
    {
        if (auto alarm = monitor->currentAlarm())
            alarms.push_back(std::move(*alarm));
    }

    return alarms;
}

void ValueGroupMonitor::setRules(const api::metrics::ValueGroupRules& rules)
{
    NX_MUTEX_LOCKER locker(&m_mutex);

    // TODO: Clean up m_valueMonitors.
    for (const auto& [parameterId, rule]: rules)
    {
        if (rule.calculate.isEmpty())
            continue;
        try
        {
            auto formula = parseFormulaOrThrow(rule.calculate, m_valueMonitors);
            m_valueMonitors.emplace(
                parameterId,
                std::make_unique<ExtraValueMonitor>(std::move(formula)));
        }
        catch (const std::exception& error)
        {
            NX_ASSERT(false, "Unable to attach extra value to %1: %2", parameterId, error.what());
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
            catch (const std::exception& error)
            {
                NX_ASSERT(
                    false, "Unable to attach alarm monitor to %1: %2", parameterId, error.what());
            }
        }
    }
}

} // namespace nx::vms::utils::metrics
