#include "rule_monitors.h"

#include <QtCore/QJsonArray>

#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/string_template.h>

#include "value_group_monitor.h"

namespace nx::vms::utils::metrics {

using Value = api::metrics::Value;

namespace {

static const QString kVariableMark("%");

template<typename Duration>
double seconds(Duration duration)
{
    return double(duration.count()) * double(Duration::period::num) / double(Duration::period::den);
}

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
        if (m_parts.size() > index)
            return m_parts[index];

        throw std::domain_error("Missing parameter in formula: " + m_formula.toStdString());
    }

    ValueMonitor* monitor(int index) const
    {
        const auto id = part(index);
        if (id.startsWith(kVariableMark))
            return monitor(id.mid(kVariableMark.size()));

        throw std::domain_error("Expected parameter instead of value in formula: "
            + m_formula.toStdString());
    }

    ValueMonitor* monitor(const QString& id) const
    {
        if (const auto it = m_monitors.find(id); it != m_monitors.end())
        {
            const auto m = it->second.get();
            m_isLocal |= (m->scope() == Scope::local);
            return m;
        }

        throw std::invalid_argument("Unknown value id: " + id.toStdString());
    }

    ValueGenerator value(int index) const { return value(part(index)); }
    ValueGenerator value(const QString& id) const
    {
        if (id.startsWith(kVariableMark))
            return [m = monitor(id.mid(kVariableMark.size()))] { return m->value(); };

        bool isNumber = false;
        if (const auto n = id.toDouble(&isNumber); isNumber)
            return [n] { return Value(n); };

        return [id] { return Value(id); };
    }

    template<typename Operation> //< Value(Value, Value)
    ValueGenerator binaryOperation(int firstI, int secondI, Operation operation) const
    {
        return
            [operation, getter1 = value(firstI), getter2 = value(secondI)]
            {
                // Currently all of the values are optional. If one of them is missing, the formula
                // result should be missing too.
                // TODO: Add optionality marker to all formulas and report error if any of
                // non-optional values is missing.
                const Value v1 = getter1();
                const Value v2 = getter2();
                return (v1.isNull() || v2.isNull())
                    ? Value() //< No value if 1 of arguments is missing.
                    : Value(operation(std::move(v1), std::move(v2)));
            };
    }

    template<typename Operation> //< Value(double, double)
    ValueGenerator numericOperation(int firstI, int secondI, Operation operation) const
    {
        return binaryOperation(
            firstI, secondI,
            [operation](Value v1, Value v2)
            {
                // Currenlty we do not have any way of reporting errors to the user. Returning the
                // error string is the only option for error detection during manual testing. End
                // users will newer see them, because rules newer change and we test our product
                // properly :).
                // TODO: Implement some kind of error handling, visible to the user.
                return (!v1.isDouble() || !v2.isDouble())
                    ? Value("ERROR: One of the arguments is not an integer")
                    : Value(operation(v1.toDouble(), v2.toDouble()));
            });
    }

    static auto square(Value value)
    {
        const auto params = value.toString().split(L'x');
        if (params.size() == 2)
            return params[0].toInt() * params[1].toInt();

        return 0;
    }

    ValueGenerator getBinaryOperation() const
    {
        if (function() == "+" || function() == "add")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 + v2; });

        if (function() == "-" || function() == "sub")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 - v2; });

        if (function() == "=" || function() == "equal")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return v1 == v2; });

        if (function() == "!=" || function() == "notEqual")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return v1 != v2; });

        if (function() == ">" || function() == "greaterThan")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 > v2; });

        if (function() == "resolutionGreaterThan")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return square(v1) > square(v2); });

        if (function() == "<" || function() == "lessThan")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 < v2; });

        if (function() == ">=" || function() == "greaterOrEqual")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 >= v2; });

        if (function() == "<=" || function() == "lessOrEqual")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 <= v2; });

        if (function() == "&&" || function() == "and")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return v1.toBool() && v2.toBool(); });

        if (function() == "||" || function() == "or")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return v1.toBool() || v2.toBool(); });

        return nullptr;
    }

    template<typename Operation> //< Value(void(Value, std::chrono::duration) forEach)
    ValueGenerator durationOperation(
        int valueI, int durationI, Border border, Operation operation) const
    {
        return
            [operation = std::move(operation), monitor = monitor(valueI),
                getDuration = value(durationI), border]
            {
                const QString durationStr = getDuration().toVariant().toString();
                const auto duration = nx::utils::parseTimerDuration(durationStr);
                if (duration.count() <= 0)
                {
                    // TODO: Error handling.
                    return Value("ERROR: Wrong duration: " + durationStr);
                }

                return Value(operation(
                    [&monitor, &duration, border](const auto& action)
                    {
                        monitor->forEach(duration, action, border);
                    }));
            };
    }

    template<typename Extraction> // double(double value, double durationS)
    ValueGenerator durationAggregation(
        int valueI, int durationI, Border border, Extraction extract,
        bool divideByTime = false) const
    {
        return durationOperation(
            valueI, durationI, border,
            [border, divideByTime, extract = std::move(extract)](const auto& forEach)
            {
                double totalValue = 0;
                double totalDurationS = 0;
                forEach(
                    [&](const Value& value, std::chrono::milliseconds duration)
                    {
                        if (value == Value())
                            return;

                        const double durationS = seconds(duration);
                        totalDurationS += durationS;
                        totalValue += extract(value.toDouble(), durationS);
                    });

                if (divideByTime)
                    return (totalDurationS == 0) ? Value() : (totalValue / totalDurationS);

                return Value(totalValue);
            });
    }

    ValueGenerator getDurationOperation() const
    {
        if (function() == "history")
        {
            return durationOperation(
                1, 2, Border::move(),
                [](const auto& forEach)
                {
                    QJsonArray items;
                    forEach([&](auto v, auto d) { items.append(QJsonArray() << v << ::toString(d)); });
                    return items;
                });
        }

        if (function() == "count")
            return durationAggregation(1, 2, Border::drop(), [](double, double) { return 1; });

        if (function() == "countValues")
        {
            return durationOperation(
                1, 2, Border::drop(),
                [expected = value(3)](const auto& forEach)
                {
                    int count = 0;
                    forEach([&](auto v, auto) { if (v == expected()) count += 1; });
                    return count;
                });
        }

        if (function() == "sum")
            return durationAggregation(1, 2, Border::drop(), [](double v, double) { return v; });

        if (function() == "average")
        {
            return durationAggregation(
                1, 2, Border::move(), [](double v, double d) { return v * d; },
                /*divideByTime*/ true);
        }

        if (function() == "perSecond")
        {
            return durationAggregation(
                1, 2, Border::hardcode(0), [](double v, double) { return v; },
                /*divideByTime*/ true);
        }

        return nullptr;
    }

    ValueGenerator build() const
    {
        m_isLocal = false;

        if (function() == "const")
            return value(1);

        if (auto generator = getBinaryOperation())
            return generator;

        if (auto generator = getDurationOperation())
            return generator;

        throw std::domain_error("Unsupported function: " + function().toStdString());
    }

    bool isLocal() const { return m_isLocal; }

private:
    const QString& m_formula;
    const QStringList m_parts;
    const ValueMonitors& m_monitors;
    mutable bool m_isLocal = false;
};

} // namespace

ValueGeneratorResult parseFormula(const QString& formula, const ValueMonitors& monitors)
{
    try
    {
        return parseFormulaOrThrow(formula, monitors);
    }
    catch (const std::domain_error& error)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Unable to parse formula '%1': %2", formula, error.what());
        return ValueGeneratorResult{nullptr, Scope::local};
    }
}

ValueGeneratorResult parseFormulaOrThrow(const QString& formula, const ValueMonitors& monitors)
{
    FormulaBuilder builder(formula, monitors);
    auto parsed = builder.build();
    return {std::move(parsed), builder.isLocal() ? Scope::local : Scope::system};
}

TextGenerator parseTemplate(QString template_, const ValueMonitors& monitors)
{
    const auto value =
        [monitors = &monitors](const QString& name) -> QString
        {
            if (const auto it = monitors->find(name); it != monitors->end())
                return it->second->formattedValue().toVariant().toString();

            return nx::utils::log::makeMessage("{%1 IS NOT FOUND}", name);
        };

    return
        [template_ = std::move(template_), value]()
        {
            return nx::utils::stringTemplate(template_, kVariableMark, value);
        };
}

ExtraValueMonitor::ExtraValueMonitor(ValueGeneratorResult formula):
    ValueMonitor(formula.scope),
    m_generator(std::move(formula.generator))
{
}

void ExtraValueMonitor::setGenerator(ValueGenerator generator)
{
    m_generator = std::move(generator);
}

api::metrics::Value ExtraValueMonitor::value() const
{
    return m_generator();
}

void ExtraValueMonitor::forEach(
    Duration maxAge, const ValueIterator& iterator, Border /*border*/) const
{
    iterator(value(), maxAge);
}

AlarmMonitor::AlarmMonitor(
    api::metrics::AlarmLevel level,
    ValueGeneratorResult condition,
    TextGenerator text)
:
    m_scope(condition.scope),
    m_level(std::move(level)),
    m_condition(std::move(condition.generator)),
    m_text(std::move(text))
{
}

std::optional<api::metrics::Alarm> AlarmMonitor::alarm()
{
    if (!m_condition().toBool())
        return std::nullopt;

    return api::metrics::Alarm{m_level, m_text()};
}

} // namespace nx::vms::utils::metrics
