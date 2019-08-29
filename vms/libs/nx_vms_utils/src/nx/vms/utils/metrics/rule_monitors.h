#pragma once

#include "value_monitors.h"

namespace nx::vms::utils::metrics {

using ValueGenerator = std::function<api::metrics::Value()>;

ValueGenerator parseFormula(const QString& formula, const ValueMonitors& monitors);
ValueGenerator parseFormulaOrThrow(const QString& formula, const ValueMonitors& monitors);

using TextGenerator = std::function<QString()>;

TextGenerator parseTemplate(QString template_, const ValueMonitors& monitors);

/**
 * Calculates value for monitoring.
 * TODO: Consider to inherit ValueMonitor, so ExtraValueMonitor could use itself.
 */
class ExtraValueMonitor: public ValueMonitor
{
public:
    ExtraValueMonitor(ValueGenerator formula);
    api::metrics::Value current() const override;

private:
    const ValueGenerator m_formula;
};

using ExtraValueMonitors = class std::map<QString, std::unique_ptr<ExtraValueMonitor>>;

/**
 * Generates alarm of condition is triggered.
 */
class AlarmMonitor
{
public:
    AlarmMonitor(QString parameter, QString level, ValueGenerator condition, TextGenerator text);
    std::optional<api::metrics::Alarm> currentAlarm();

private:
    const QString m_parameter;
    const QString m_level;
    const ValueGenerator m_condition;
    const TextGenerator m_text;
};

using AlarmMonitors = std::vector<std::unique_ptr<AlarmMonitor>>;

} // namespace nx::vms::utils::metrics
