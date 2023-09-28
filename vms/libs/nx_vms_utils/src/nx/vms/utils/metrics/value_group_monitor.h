// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/dot_notation_string.h>
#include <nx/utils/value_history.h>
#include <nx/vms/api/metrics.h>

#include "rule_monitors.h"
#include "value_monitors.h"

namespace nx::vms::utils::metrics {

/**
 * Provides values for parameter group.
 */
class NX_VMS_UTILS_API ValueGroupMonitor
{
public:
    ValueGroupMonitor(ValueMonitors monitors);

    api::metrics::ValueGroup values(Scope requiredScope, bool formatted) const;
    api::metrics::ValueGroupAlarms alarms(Scope requiredScope) const;

    std::pair<api::metrics::ValueGroup, Scope> valuesWithScope(Scope requiredScope,
        bool formatted,
        const nx::utils::DotNotationString& filter = {}) const;
    std::pair<api::metrics::ValueGroupAlarms, Scope> alarmsWithScope(Scope requiredScope,
        const nx::utils::DotNotationString& filter = {}) const;

    void setRules(
        const std::map<QString, api::metrics::ValueRule>& rules,
        bool skipOnMissingArgument = false);

private:
    void updateExtraValue(
        const QString& parameterId, const api::metrics::ValueRule& rule, bool skipOnMissingArgument);
    void updateAlarms(
        const QString& parameterId, const api::metrics::ValueRule& rule, bool skipOnMissingArgument);

private:
    mutable nx::Mutex m_mutex;
    ValueMonitors m_valueMonitors;
    AlarmMonitors m_alarmMonitors;
};

using ValueGroupMonitors = std::map<QString, std::unique_ptr<ValueGroupMonitor>>;

} // namespace nx::vms::utils::metrics
