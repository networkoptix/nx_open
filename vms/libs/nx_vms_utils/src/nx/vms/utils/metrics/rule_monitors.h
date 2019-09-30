#pragma once

#include "value_monitors.h"

namespace nx::vms::utils::metrics {

using ValueGenerator = std::function<api::metrics::Value()>;

NX_VMS_UTILS_API ValueGenerator parseFormula(const QString& formula, const ValueMonitors& monitors);
NX_VMS_UTILS_API ValueGenerator parseFormulaOrThrow(const QString& formula, const ValueMonitors& monitors);

using TextGenerator = std::function<QString()>;

NX_VMS_UTILS_API TextGenerator parseTemplate(QString template_, const ValueMonitors& monitors);

/**
 * Calculates value for monitoring.
 */
class NX_VMS_UTILS_API ExtraValueMonitor: public ValueMonitor
{
public:
    ExtraValueMonitor(ValueGenerator formula);
    api::metrics::Value current() const override;
    void forEach(Duration maxAge, const ValueIterator& iterator) const override;

private:
    const ValueGenerator m_formula;
};

/**
 * Generates alarm of condition is triggered.
 */
class NX_VMS_UTILS_API AlarmMonitor
{
public:
    AlarmMonitor(
        QString parameter,
        api::metrics::AlarmLevel level,
        ValueGenerator condition,
        TextGenerator text);

    std::optional<api::metrics::Alarm> currentAlarm();

private:
    const QString m_parameter;
    const api::metrics::AlarmLevel m_level;
    const ValueGenerator m_condition;
    const TextGenerator m_text;
};

using AlarmMonitors = std::vector<std::unique_ptr<AlarmMonitor>>;

} // namespace nx::vms::utils::metrics
