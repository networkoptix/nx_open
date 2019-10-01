#pragma once

#include <memory>

#include <nx/vms/api/metrics.h>
#include <nx/utils/value_history.h>

#include "value_monitors.h"
#include "rule_monitors.h"

namespace nx::vms::utils::metrics {

/**
 * Provides values for parameter group.
 */
class NX_VMS_UTILS_API ValueGroupMonitor
{
public:
    ValueGroupMonitor(ValueMonitors monitors);

    api::metrics::ValueGroup values(Scope requiredScope, bool formatted) const;
    std::vector<api::metrics::Alarm> alarms(Scope requiredScope) const;

    void setRules(
        const std::map<QString, api::metrics::ValueRule>& rules,
        bool skipOnMissingArgument = false);

private:
    void updateExtraValue(
        const QString& parameterId, const api::metrics::ValueRule& rule, bool skipOnMissingArgument);
    void updateAlarms(
        const QString& parameterId, const api::metrics::ValueRule& rule, bool skipOnMissingArgument);

private:
    mutable nx::utils::Mutex m_mutex;
    ValueMonitors m_valueMonitors;
    AlarmMonitors m_alarmMonitors;
};

using ValueGroupMonitors = std::map<QString, std::unique_ptr<ValueGroupMonitor>>;

} // namespace nx::vms::utils::metrics


