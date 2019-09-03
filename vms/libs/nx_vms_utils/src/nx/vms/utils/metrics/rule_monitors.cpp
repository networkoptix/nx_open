#include "rule_monitors.h"

#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/string_template.h>

#include "value_group_monitor.h"

namespace nx::vms::utils::metrics {

namespace {

class FormulaBuilder
{
public:
    FormulaBuilder(const QString& formula, const ValueMonitors& monitors):
        m_formula(formula),
        m_parts(formula.split(' ')),
        m_monitors(monitors)
    {
    }

    const QString& function() const { return part(0); }
    const QString& part(int index) const
    {
        if (m_parts.size() <= index)
            throw std::domain_error("Missing parameter in formula: " + m_formula.toStdString());
        return m_parts[index];
    }

    ValueMonitor* monitor(int index) const
    {
        const auto id = part(index);
        if (const auto it = m_monitors.find(id); it != m_monitors.end())
            return it->second.get();
        throw std::domain_error("Unknown parameter: " + id.toStdString());
    }

    std::chrono::milliseconds duration(int index) const
    {
        const auto value = part(index);
        const auto parsed = nx::utils::parseTimerDuration(part(index));
        if (parsed.count())
            return parsed;
        throw std::domain_error("Invalid duration: " + value.toStdString());
    }

    template<typename Operation> // api::metrics::Value(api::metrics::Value, api::metrics::Value)
    ValueGenerator binaryOperatiron(Operation operation) const
    {
        return
            [operation, m1 = monitor(1), m2 = monitor(2)]
            {
                auto v1 = m1->current();
                auto v2 = m2->current();
                return (v1.isNull() || v2.isNull())
                    ? api::metrics::Value()
                    : api::metrics::Value(operation(std::move(v1), std::move(v2)));
            };
    }

    template<typename Operation> // double(double, double)
    ValueGenerator numericOperatiron(Operation operation) const
    {
        return binaryOperatiron(
            [operation](api::metrics::Value v1, api::metrics::Value v2)
            {
                return (!v1.isDouble() || !v2.isDouble())
                    ? api::metrics::Value("ERROR: One of the arguments is not an integer")
                    : api::metrics::Value(operation(v1.toDouble(), v2.toDouble()));
            });
    }

    template<typename Operation>
    ValueGenerator last(Operation operation) const
    {
        return
            [operation, m = monitor(1), d = duration(2)]
            {
                const auto values = m->last(d);
                return api::metrics::Value(operation(values));
            };
    }

    ValueGenerator build() const
    {
        const auto p1 = part(1);
        if (function() == "s" || function() == "string")
            return [s = part(1)] { return api::metrics::Value(s); };

        if (function() == "n" || function() == "number")
            return [n = part(1).toDouble()] { return api::metrics::Value(n); };

        if (function() == "+" || function() == "add")
            return numericOperatiron([](auto v1, auto v2) { return v1 + v2; });

        if (function() == "-" || function() == "sub")
            return numericOperatiron([](auto v1, auto v2) { return v1 - v2; });

        if (function() == "c" || function() == "count")
            return last([](const auto& values) { return (double) values.size(); });

        if (function() == "=" || function() == "eq")
            return binaryOperatiron([](auto v1, auto v2) { return v1 == v2; });

        if (function() == "!=" || function() == "ne")
            return binaryOperatiron([](auto v1, auto v2) { return v1 != v2; });

        if (function() == ">" || function() == "gt")
            return numericOperatiron([](auto v1, auto v2) { return v1 > v2; });

        if (function() == "<" || function() == "lt")
            return numericOperatiron([](auto v1, auto v2) { return v1 < v2; });

        if (function() == ">=" || function() == "ge")
            return numericOperatiron([](auto v1, auto v2) { return v1 >= v2; });

        if (function() == "<=" || function() == "le")
            return numericOperatiron([](auto v1, auto v2) { return v1 <= v2; });

        throw std::domain_error("Unsupported function: " + function().toStdString());
    }

private:
    const QString& m_formula;
    const QStringList m_parts;
    const ValueMonitors& m_monitors;
};

} // namespace

ValueGenerator parseFormula(const QString& formula, const ValueMonitors& monitors)
{
    try
    {
        return parseFormulaOrThrow(formula, monitors);
    }
    catch (const std::domain_error& error)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Unable to parse formula '%1': %2", formula, error.what());
        return nullptr;
    }
}

ValueGenerator parseFormulaOrThrow(const QString& formula, const ValueMonitors& monitors)
{
    return FormulaBuilder(formula, monitors).build();
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
