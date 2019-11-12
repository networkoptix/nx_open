#pragma once

#include "value_monitors.h"

namespace nx::vms::utils::metrics {

// General rules file parsing error.
class RuleSyntaxError: public MetricsError
{
    using MetricsError::MetricsError;
};

class UnknownValueId: public RuleSyntaxError
{
    using RuleSyntaxError::RuleSyntaxError;
};

// General formula calculation error in ExtraValueMonitor.
class FormulaCalculationError: public MetricsError
{
   using MetricsError::MetricsError;
};

class NullValueError: public FormulaCalculationError
{
    using FormulaCalculationError::FormulaCalculationError;
};

// NOTE: Generators are allowed to throw exceptions!
using ValueGenerator = std::function<api::metrics::Value()>;

struct ValueGeneratorResult
{
    ValueGenerator generator;
    Scope scope = Scope::local;
};

NX_VMS_UTILS_API ValueGeneratorResult parseFormula(
    const QString& formula, const ValueMonitors& monitors);
NX_VMS_UTILS_API ValueGeneratorResult parseFormulaOrThrow(
    const QString& formula, const ValueMonitors& monitors);

// NOTE: Generators are allowed to throw exceptions!
using TextGenerator = std::function<QString()>;

NX_VMS_UTILS_API TextGenerator parseTemplate(QString template_, const ValueMonitors& monitors);

/**
 * Calculates value for monitoring.
 */
class NX_VMS_UTILS_API ExtraValueMonitor: public ValueMonitor
{
public:
    ExtraValueMonitor(QString name, ValueGeneratorResult formula = {});
    void setGenerator(ValueGenerator generator);

    void forEach(Duration maxAge, const ValueIterator& iterator, Border border) const override;

protected:
    virtual api::metrics::Value valueOrThrow() const override;
private:
    ValueGenerator m_generator;
};

/**
 * Generates alarm if condition is triggered.
 */
class NX_VMS_UTILS_API AlarmMonitor
{
public:
    AlarmMonitor(
        api::metrics::AlarmLevel level,
        ValueGeneratorResult condition,
        TextGenerator text);

    Scope scope() const { return m_scope; }
    std::optional<api::metrics::Alarm> alarm();

private:
    const Scope m_scope = Scope::local;
    const api::metrics::AlarmLevel m_level;
    const ValueGenerator m_condition;
    const TextGenerator m_textGenerator;
};

using AlarmMonitors = std::map<QString, std::vector<std::unique_ptr<AlarmMonitor>>>;

} // namespace nx::vms::utils::metrics
