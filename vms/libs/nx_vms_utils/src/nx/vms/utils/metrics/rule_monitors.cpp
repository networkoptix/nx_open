#include "rule_monitors.h"

#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/string_template.h>

#include "value_group_monitor.h"

namespace nx::vms::utils::metrics {

ValueGenerator parseFormula(const QString& formula, const ValueMonitors& monitors)
{
    try
    {
        return parseFormulaOrThrow(formula, monitors);
    }
    catch (const std::domain_error& error)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Unable to parse formule '%1': %2", formula, error.what());
        return nullptr;
    }
}

ValueGenerator parseFormulaOrThrow(const QString& formula, const ValueMonitors& monitors)
{
    const auto parts = formula.splitRef(' ');
    const auto part =
        [&](int index)
        {
            if (parts.size() < index)
                throw std::domain_error("Missing parameter in formula: " + formula.toStdString());
            return parts[index].toString();
        };

    using Value = api::metrics::Value;
    const auto monitor =
        [&](int index)
        {
            const auto id = part(index);
            if (const auto it = monitors.find(id); it != monitors.end())
                return it->second.get();
            throw std::domain_error("Unknown parameter: " + id.toStdString());
        };

    const auto interval =
        [&](int index)
        {
            const auto value = part(index);
            const auto parsed = nx::utils::parseTimerDuration(part(index));
            if (parsed.count())
                return parsed;
            throw std::domain_error("Invalid duration: " + value.toStdString());
        };

    const auto function = part(0);
    if (function == "n" || function == "number")
        return [n = part(1).toDouble()] { return Value(n); };

    if (function == "s" || function == "string")
        return [n = part(1)] { return Value(n); };

    if (function == "+" || function == "add")
        return [m1 = monitor(1), m2 = monitor(2)] { return Value(m1->currentN() + m2->currentN()); };

    if (function == "-" || function == "sub")
        return [m1 = monitor(1), m2 = monitor(2)] { return Value(m1->currentN() - m2->currentN()); };

    if (function == "c" || function == "count")
        return [m = monitor(1), i = interval(2)] { return Value((double) m->last(i).size()); };

    if (function == "a" || function == "average")
    {
        return
            [m = monitor(1), i = interval(2)]
            {
                const auto values = m->last(i);
                double sum = 0;
                for (const auto& v: values)
                    sum += v.value.toDouble(); //< Take account of interval length.
                return sum ? (sum / double(values.size())) : 0;
            };
    }

    if (function == "=" || function == "eq")
        return [m1 = monitor(1), m2 = monitor(2)] { return Value(m1->current() == m2->current()); };

    if (function == "!=" || function == "ne")
        return [m1 = monitor(1), m2 = monitor(2)] { return Value(m1->current() != m2->current()); };

    if (function == ">" || function == "gt")
        return [m1 = monitor(1), m2 = monitor(2)] { return Value(m1->currentN() > m2->currentN()); };

    if (function == "<" || function == "lt")
        return [m1 = monitor(1), m2 = monitor(2)] { return Value(m1->currentN() < m2->currentN()); };

    if (function == ">=" || function == "ge")
        return [m1 = monitor(1), m2 = monitor(2)] { return Value(m1->currentN() >= m2->currentN()); };

    if (function == "<=" || function == "le")
        return [m1 = monitor(1), m2 = monitor(2)] { return Value(m1->currentN() <= m2->currentN()); };

    throw std::domain_error("Unsupported function: " + function.toStdString());
}

TextGenerator parseTemplate(QString template_, const ValueMonitors& monitors)
{
    // TODO: Cache up template before hand.
    const auto value =
        [monitors = &monitors](const QString& name) -> QString
        {
            if (const auto it = monitors->find(name); it != monitors->end())
            {
                // TODO: Introduce some kind of a fancy formatting?
                return it->second->current().toVariant().toString();
            }
            return nx::utils::log::makeMessage("{%1 IS NOT FOUND}", name);
        };

    return [t = std::move(template_), value]() { return nx::utils::stringTemplate(t, value); };
}

ExtraValueMonitor::ExtraValueMonitor(ValueGenerator formula):
    m_formula(std::move(formula))
{
}

api::metrics::Value ExtraValueMonitor::current() const
{
    return m_formula();
}

AlarmMonitor::AlarmMonitor(
    QString parameter, QString level, ValueGenerator condition, TextGenerator text)
:
    m_parameter(std::move(parameter)),
    m_level(std::move(level)),
    m_condition(std::move(condition)),
    m_text(std::move(text))
{
}

std::optional<api::metrics::Alarm> AlarmMonitor::currentAlarm()
{
    if (!m_condition().toBool())
        return std::nullopt;

    return api::metrics::Alarm{"UNKNOWN", m_parameter, m_level, m_text()};
}

} // namespace nx::vms::utils::metrics
