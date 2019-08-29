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
class ValueGroupMonitor
{
public:
    ValueGroupMonitor(ValueMonitors monitors);

    api::metrics::ValueGroup current() const;
    std::vector<api::metrics::Alarm> alarms() const;

    void setRules(const api::metrics::ValueGroupRules& rules);

private:
    mutable nx::utils::Mutex m_mutex;
    ValueMonitors m_valueMonitors;
    AlarmMonitors m_alarmMonitors;
};

using ValueGroupMonitors = std::map<QString, std::unique_ptr<ValueGroupMonitor>>;

} // namespace nx::vms::utils::metrics


