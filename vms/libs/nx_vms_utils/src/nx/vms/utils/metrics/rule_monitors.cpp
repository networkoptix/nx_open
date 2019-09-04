#include "rule_monitors.h"

#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/string_template.h>

#include "value_group_monitor.h"

namespace nx::vms::utils::metrics {

namespace {

static const QString kVariableMark("%");

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

    ValueMonitor* monitor(int index) const { return monitor(part(index)); }
    ValueMonitor* monitor(const QString& id) const
    {
        if (const auto it = m_monitors.find(id); it != m_monitors.end())
            return it->second.get();
        throw std::domain_error("Unknown parameter: " + id.toStdString());
    }

    ValueGenerator value(int index) const { return value(part(index)); }
    ValueGenerator value(const QString& id) const
    {
        if (id.startsWith(kVariableMark))
            return [m = monitor(id.mid(kVariableMark.size()))] { return m->current(); };

        bool isNumber = false;
        if (const auto n = id.toDouble(&isNumber); isNumber)
            return [n] { return api::metrics::Value(n); };

        return [id] { return api::metrics::Value(id); };
    }

    template<typename Operation> //< Value(Value, Value)
    ValueGenerator binaryOperation(int firstI, int secondI, Operation operation) const
    {
        return
            [operation, getter1 = value(firstI), getter2 = value(secondI)]
            {
                auto v1 = getter1();
                auto v2 = getter2();
                return (v1.isNull() || v2.isNull())
                    ? api::metrics::Value() //< No value if 1 of arguments is missing.
                    : api::metrics::Value(operation(std::move(v1), std::move(v2)));
            };
    }

    template<typename Operation> //< Value(double, double)
    ValueGenerator numericOperation(int firstI, int secondI, Operation operation) const
    {
        return binaryOperation(
            firstI, secondI,
            [operation](api::metrics::Value v1, api::metrics::Value v2)
            {
                return (!v1.isDouble() || !v2.isDouble())
                    ? api::metrics::Value("ERROR: One of the arguments is not an integer")
                    : api::metrics::Value(operation(v1.toDouble(), v2.toDouble()));
            });
    }

    template<typename Operation> //< Value(std::vector<TimedValue<Value>>)
    ValueGenerator durationOperation(int valueI, int durationI, Operation operation) const
    {
        return
            [operation, monitor = monitor(valueI), getDuration = value(durationI)]
            {
                const auto durationStr = getDuration().toVariant().toString();
                const auto duration = nx::utils::parseTimerDuration(durationStr);
                if (duration.count() <= 0)
                    return api::metrics::Value("ERROR: Wrong duration: " + durationStr);

                auto values = monitor->last(duration);
                if (values.empty())
                    return api::metrics::Value(); // No value if duration does not contain any data.

                return api::metrics::Value(operation(std::move(values)));
            };
    }

    ValueGenerator buildArithmetic() const
    {
        if (function() == "c" || function() == "const") // value
            return value(1);

        if (function() == "+" || function() == "add") // first second
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 + v2; });

        if (function() == "-" || function() == "sub") // first second
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 - v2; });

        if (function() == "c" || function() == "count") // value duration
            return durationOperation(1, 2, [](auto values) { return (double) values.size(); });

        if (function() == "cif" || function() == "countIf") // value duration expected
        {
            return durationOperation(
                1, 2,
                [condition = value(3)](auto values)
                {
                    const auto expected = condition();
                    size_t count = 0;
                    for (const auto& v: values) if (v.value == expected) count++;
                    return (double) values.size();
                });
        }

        return nullptr;
    }

    ValueGenerator buildConditional() const
    {
        if (function() == "=" || function() == "eq")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return v1 == v2; });

        if (function() == "!=" || function() == "ne")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return v1 != v2; });

        if (function() == ">" || function() == "gt")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 > v2; });

        if (function() == "<" || function() == "lt")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 < v2; });

        if (function() == ">=" || function() == "ge")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 >= v2; });

        if (function() == "<=" || function() == "le")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 <= v2; });

        return nullptr;
    }

    ValueGenerator build() const
    {
        if (auto generator = buildArithmetic())
            return generator;

        if (auto generator = buildConditional())
            return generator;

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

    return
        [template_ = std::move(template_), value]()
        {
            return nx::utils::stringTemplate(template_, kVariableMark, value);
        };
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
