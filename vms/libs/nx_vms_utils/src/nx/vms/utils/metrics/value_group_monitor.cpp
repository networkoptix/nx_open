#include "value_group_monitor.h"

#include "rule_monitors.h"

namespace nx::vms::utils::metrics {

ValueGroupMonitor::ValueGroupMonitor(ValueMonitors monitors):
    m_valueMonitors(std::move(monitors))
{
}

api::metrics::ValueGroup ValueGroupMonitor::values(Scope requiredScope, bool formatted) const
{
    // TODO: Use RW lock.
    NX_MUTEX_LOCKER locker(&m_mutex);

    api::metrics::ValueGroup values;
    for (const auto& [id, monitor]: m_valueMonitors)
    {
        if (requiredScope == Scope::local && monitor->scope() == Scope::system)
            continue;

        auto value = formatted ? monitor->formattedValue() : monitor->value();
        if (!value.isNull())
            values[id] = std::move(value);
    }

    return values;
}

api::metrics::ValueGroupAlarms ValueGroupMonitor::alarms(Scope requiredScope) const
{
    // TODO: Use RW lock.
    NX_MUTEX_LOCKER locker(&m_mutex);

    api::metrics::ValueGroupAlarms alarms;
    for (const auto& [id, monitors]: m_alarmMonitors)
    {
        for (const auto& monitor: monitors)
        {
            if (requiredScope == Scope::local && monitor->scope() == Scope::system)
                continue;

            if (auto alarm = monitor->alarm())
                alarms[id].push_back(std::move(*alarm));
        }
    }

    return alarms;
}

void ValueGroupMonitor::setRules(
    const std::map<QString, api::metrics::ValueRule>& rules, bool skipOnMissingArgument)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    for (auto it = m_valueMonitors.begin(); it != m_valueMonitors.end();)
    {
        if (dynamic_cast<ExtraValueMonitor*>(it->second.get()))
            it = m_valueMonitors.erase(it);
        else
            ++it;
    }

    // Prepare monitors for extra values to make sure all of the formulas will find their arguments.
    for (const auto& [id, rule]: rules)
    {
        if (!rule.calculate.isEmpty())
            m_valueMonitors.emplace(id, std::make_unique<ExtraValueMonitor>(id));

        NX_ASSERT(m_valueMonitors.count(id) || skipOnMissingArgument, "Unknown id in rules: %1", id);
    }
    for (const auto& [id, rule]: rules)
    {
        if (!rule.calculate.isEmpty())
            updateExtraValue(id, rule, skipOnMissingArgument);
    }

    for (const auto& [id, monitor]: m_valueMonitors)
    {
        const auto rule = rules.find(id);
        monitor->setFormatter(api::metrics::makeFormatter(
            rule != rules.end() ? rule->second.format : QString()));
    }

    m_alarmMonitors.clear();
    for (const auto& [id, rule]: rules)
        updateAlarms(id, rule, skipOnMissingArgument);
}

void ValueGroupMonitor::updateExtraValue(
    const QString& parameterId, const api::metrics::ValueRule& rule, bool skipOnMissingArgument)
{
    auto monitor = dynamic_cast<ExtraValueMonitor*>(m_valueMonitors[parameterId].get());
    NX_CRITICAL(monitor);
    try
    {
        try
        {
            auto formula = parseFormulaOrThrow(rule.calculate, m_valueMonitors);
            monitor->setGenerator(std::move(formula.generator));
            monitor->setScope(formula.scope);
        }
        catch (const std::invalid_argument& error)
        {
            if (!skipOnMissingArgument) throw;
            NX_DEBUG(this, "Skip extra value %1: %2", parameterId, error.what());
            m_valueMonitors.erase(parameterId);
        }
    }
    catch (const std::logic_error& error)
    {
        NX_ASSERT(false, "Unable to add extra value %1: %2", parameterId, error.what());
        m_valueMonitors.erase(parameterId);
    }
}

void ValueGroupMonitor::updateAlarms(
    const QString& parameterId, const api::metrics::ValueRule& rule, bool skipOnMissingArgument)
{
    for (const auto& alarmRule: rule.alarms)
    {
        try
        {
            try
            {
                m_alarmMonitors[parameterId].push_back(std::make_unique<AlarmMonitor>(
                    alarmRule.level,
                    parseFormulaOrThrow(alarmRule.condition, m_valueMonitors),
                    parseTemplate(alarmRule.text, m_valueMonitors)
                ));
            }
            catch (const std::invalid_argument& error)
            {
                if (!skipOnMissingArgument) throw;
                NX_DEBUG(this, "Skip alarm monitor %1: %2", parameterId, error.what());
            }
        }
        catch (const std::logic_error& error)
        {
            NX_ASSERT(false, "Unable to add alarm monitor %1: %2", parameterId, error.what());
        }
    }
}

} // namespace nx::vms::utils::metrics
