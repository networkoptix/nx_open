#pragma once

#include "value_group_monitor.h"

namespace nx::vms::utils::metrics {

/**
 * Provides values for entire resource.
 */
class NX_VMS_UTILS_API ResourceMonitor
{
public:
    ResourceMonitor(std::unique_ptr<ResourceDescription> resource, ValueGroupMonitors monitors);

    QString id() const { return m_resource->id; }

    api::metrics::ResourceValues values(Scope requestScope, bool formatted) const;
    std::vector<api::metrics::Alarm> alarms(Scope requestScope) const;

    void setRules(const api::metrics::ResourceRules& rules);
    QString idForToStringFromPtr() const;

private:
    const std::unique_ptr<ResourceDescription> m_resource;
    ValueGroupMonitors m_monitors;
};

} // namespace nx::vms::utils::metrics
