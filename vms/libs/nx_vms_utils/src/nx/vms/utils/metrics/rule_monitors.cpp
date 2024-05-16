// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rule_monitors.h"

#include <QtCore/QJsonArray>

#include <nx/utils/log/log.h>
#include <nx/utils/string_template.h>
#include <nx/utils/timer_manager.h>

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

        throw RuleSyntaxError("Missing parameter in formula: " + m_formula.toStdString());
    }

    ValueMonitor* monitor(int index) const
    {
        const auto id = part(index);
        if (id.startsWith(kVariableMark))
            return monitor(id.mid(kVariableMark.size()));

        throw RuleSyntaxError("Expected parameter instead of value in formula: "
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

        throw UnknownValueId("Unknown value id: " + id.toStdString());
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
                const Value v1 = getter1();
                const Value v2 = getter2();
                if (v1.isNull() || v2.isNull())
                    throw NullValueError("At least one argument is missing");

                return Value(operation(std::move(v1), std::move(v2)));
            };
    }

    template<typename Operation> //< Value(double, double)
    ValueGenerator numericOperation(int firstI, int secondI, Operation operation) const
    {
        return binaryOperation(
            firstI, secondI,
            [operation](Value v1, Value v2)
            {
                if (!v1.isDouble() || !v2.isDouble())
                    throw FormulaCalculationError("At least one argument is not a number");
                return Value(operation(v1.toDouble(), v2.toDouble()));
            });
    }

    template<typename Operation> //< Value(bool, bool)
    ValueGenerator boolOperation(int firstI, int secondI, Operation operation) const
    {
        return binaryOperation(
            firstI, secondI,
            [operation](Value v1, Value v2)
            {
                if (!v1.isBool() || !v2.isBool())
                    throw FormulaCalculationError("At least one argument is not a boolean");
                return Value(operation(v1.toBool(), v2.toBool()));
            });
    }

    static int square(Value value) noexcept(false)
    {
        const auto params = value.toString().split('x');
        if (params.size() != 2)
            throw FormulaCalculationError("Invalid resolution size syntax");

        bool isOk1 = false;
        bool isOk2 = false;
        int result = params[0].toInt(&isOk1) * params[1].toInt(&isOk2);
        if (!isOk1 || !isOk2)
            throw FormulaCalculationError("Invalid resolution size syntax: integers expected");
        return result;
    }

    static bool isEqual(const Value& left, const Value& right)
    {
        if (!left.isString() || !right.isString())
            return left == right;

        // Currently formula language does not support spaces in values. The "_" can be used
        // instead to achieve desired effect.
        return left.toString().replace("_", " ") == right.toString().replace("_", " ");
    }

    ValueGenerator getBinaryOperation() const
    {
        if (function() == "+" || function() == "add")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 + v2; });

        if (function() == "-" || function() == "sub")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 - v2; });

        if (function() == "*" || function() == "multiply")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 * v2; });

        if (function() == "/" || function() == "divide")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v2 ? Value(v1 / v2) : Value(); });

        if (function() == "=" || function() == "equal")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return isEqual(v1, v2); });

        if (function() == "!=" || function() == "notEqual")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return !isEqual(v1, v2); });

        if (function() == "notNullAndNotEqual")
        {
            return binaryOperation(1, 2,
                [](auto v1, auto v2) { return !v1.isNull() && !v2.isNull() && !isEqual(v1, v2); });
        }

        if (function() == ">" || function() == "greaterThan")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 > v2; });

        if (function() == "resolutionGreaterOrEqualThan")
            return binaryOperation(1, 2, [](auto v1, auto v2) { return square(v1) >= square(v2); });

        if (function() == "<" || function() == "lessThan")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 < v2; });

        if (function() == ">=" || function() == "greaterOrEqual")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 >= v2; });

        if (function() == "<=" || function() == "lessOrEqual")
            return numericOperation(1, 2, [](auto v1, auto v2) { return v1 <= v2; });

        if (function() == "&&" || function() == "and")
            return boolOperation(1, 2, [](auto v1, auto v2) { return v1 && v2; });

        if (function() == "||" || function() == "or")
            return boolOperation(1, 2, [](auto v1, auto v2) { return v1 || v2; });

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
                    throw FormulaCalculationError("Invalid duration: " + durationStr.toStdString());
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
        bool divideByTime = false, bool mustExist = false) const
    {
        return durationOperation(
            valueI, durationI, border,
            [border, divideByTime, mustExist, extract = std::move(extract)](const auto& forEach)
            {
                double totalValue = 0;
                double totalDurationS = 0;
                Value lastValue;
                forEach(
                    [&](Value value, std::chrono::milliseconds duration)
                    {
                        lastValue = value;
                        if (value == Value())
                            return; //< Do not process empty value periods.

                        const double durationS = seconds(duration);
                        totalDurationS += durationS;
                        totalValue += extract(value.toDouble(), durationS);
                    });

                if (mustExist && lastValue == Value())
                    return Value();

                if (divideByTime)
                    return (totalDurationS == 0) ? Value() : (totalValue / totalDurationS);

                return Value(totalValue);
            });
    }

    // TODO: Refactor!
    ValueGenerator getDurationOperation() const
    {
        if (function() == "history")
        {
            return durationOperation(
                1, 2, Border::keep(),
                [](const auto& forEach)
                {
                    QJsonArray items;
                    forEach([&](auto v, auto d) { items.append(QJsonArray() << v << nx::toString(d)); });
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

        if (function() == "countValuesNotEqual")
        {
            return durationOperation(
                1, 2, Border::drop(),
                [expected = value(3)](const auto& forEach)
                {
                    int count = 0;
                    forEach([&](auto v, auto) { if (!v.isNull() && v != expected()) count += 1; });
                    return count;
                });
        }

        if (function() == "sampleAvg") //< sum(v * dt) / t
        {
            return durationAggregation(
                1, 2, Border::move(), [](double v, double d) { return v * d; },
                /*divideByTime*/ true, /*mustExist*/ true);
        }

        if (function() == "counterDelta") //< dv
        {
            return durationOperation(
                1, 2, Border::move(),
                [](const auto& forEach)
                {
                    std::optional<double> first;
                    double last = 0;
                    forEach(
                        [&](const Value& value, Duration)
                        {
                            if (value != Value())
                            {
                                last = value.toDouble();
                                if (!first)
                                    first = last;
                            }
                        });

                    return first ? Value(last - *first) : Value();
                });
        }

        if (function() == "counterToAvg") //< dv / t
        {
            return durationOperation(
                1, 2, Border::move(),
                [](const auto& forEach)
                {
                    std::optional<double> first;
                    double last = 0;
                    double totalTimeS = 0;
                    forEach(
                        [&](const Value& value, Duration duration)
                        {
                            if (value != Value())
                            {
                                last = value.toDouble();
                                if (!first)
                                    first = last;
                            }

                            totalTimeS += seconds(duration);
                        });

                    return (first && totalTimeS)  ? Value((last - *first) / totalTimeS) : Value();
                });
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

        throw RuleSyntaxError("Unsupported function: " + function().toStdString());
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
    catch (const RuleSyntaxError& error)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Unable to parse formula '%1': %2", formula, error.what());
        return ValueGeneratorResult{nullptr, Scope::local};
    }
}

ValueGeneratorResult parseFormulaOrThrow(const QString& formula, const ValueMonitors& monitors)
{
    FormulaBuilder builder(formula, monitors);
    auto parsed = builder.build();
    return {std::move(parsed), builder.isLocal() ? Scope::local : Scope::site};
}

TextGenerator parseTemplate(QString template_, const ValueMonitors& monitors)
{
    const auto value =
        [template_, monitors = &monitors](const QString& name) -> QString
        {
            if (const auto it = monitors->find(name); it != monitors->end())
                return it->second->formattedValue().toVariant().toString();

            NX_ASSERT(false, "Value [%1] is not found for template [%2]", name, template_);
            return NX_FMT("{%1 IS NOT FOUND}", name);
        };

    return
        [template_ = std::move(template_), value]()
        {
            return nx::utils::stringTemplate(template_, kVariableMark, value);
        };
}

ExtraValueMonitor::ExtraValueMonitor(QString name, ValueGeneratorResult formula):
    ValueMonitor(std::move(name), formula.scope),
    m_generator(std::move(formula.generator))
{
}

void ExtraValueMonitor::setGenerator(ValueGenerator generator)
{
    m_generator = std::move(generator);
}

api::metrics::Value ExtraValueMonitor::valueOrThrow() const
{
    try
    {
        return m_generator();
    }
    catch (const NullValueError&)
    {
        if (!optional())
            throw;
        return {};
    }
}

void ExtraValueMonitor::forEach(
    Duration maxAge, const ValueIterator& iterator, Border /*border*/) const
{
    iterator(value(), maxAge);
}

AlarmMonitor::AlarmMonitor(
    QString parentParameterId,
    bool isOptional,
    api::metrics::AlarmLevel level,
    ValueGeneratorResult condition,
    TextGenerator text)
:
    m_parentParameterId(std::move(parentParameterId)),
    m_optional(isOptional),
    m_scope(condition.scope),
    m_level(std::move(level)),
    m_condition(std::move(condition.generator)),
    m_textGenerator(std::move(text))
{
}

std::optional<api::metrics::Alarm> AlarmMonitor::alarm()
{
    try {
        if (!m_condition().toBool())
            return std::nullopt;

        return api::metrics::Alarm{m_level, m_textGenerator()};
    }
    catch (const ExpectedError& e)
    {
        NX_DEBUG(this, "Got error: %1", e);
    }
    catch (const NullValueError& e)
    {
        NX_ASSERT(m_optional, "Value %1 is not optional: %2", this, e);
    }
    catch (const BaseError& e)
    {
        NX_ASSERT(false, "Got unexpected alarm %1 error: %2", this, e);
    }
    catch (const std::exception& e)
    {
        NX_ASSERT(false, "Unexpected general error when check-in alarm %1: %2", this, e);
    }

    // TODO: Should we return error to the user if it occurs?
    return std::nullopt;
}

QString AlarmMonitor::idForToStringFromPtr() const
{
    return m_parentParameterId;
}

} // namespace nx::vms::utils::metrics
