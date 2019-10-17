#pragma once

#include "value_monitors.h"

namespace nx::vms::utils::metrics {

using ValueGenerator = std::function<api::metrics::Value()>;

struct ValueGeneratorResult
{
    ValueGenerator generator;
    Scope scope = Scope::local;
};

NX_VMS_UTILS_API ValueGeneratorResult parseFormula(const QString& formula, const ValueMonitors& monitors);
NX_VMS_UTILS_API ValueGeneratorResult parseFormulaOrThrow(const QString& formula, const ValueMonitors& monitors);

using TextGenerator = std::function<QString()>;

NX_VMS_UTILS_API TextGenerator parseTemplate(QString template_, const ValueMonitors& monitors);

/**
 * Calculates value for monitoring.
 */
class NX_VMS_UTILS_API ExtraValueMonitor: public ValueMonitor
{
public:
    ExtraValueMonitor(ValueGeneratorResult formula = {});
    void setGenerator(ValueGenerator generator);

    api::metrics::Value value() const override;
    void forEach(Duration maxAge, const ValueIterator& iterator) const override;

private:
    ValueGenerator m_generator;
};

/**
 * Generates alarm of condition is triggered.
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
    const TextGenerator m_text;
};

using AlarmMonitors = std::map<QString, std::vector<std::unique_ptr<AlarmMonitor>>>;

} // namespace nx::vms::utils::metrics
