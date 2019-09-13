#pragma once

#include "value_group_monitor.h"

namespace nx::vms::utils::metrics {

/**
 * Provides values for entire resource.
 */
class NX_VMS_UTILS_API ResourceMonitor
{
public:
    class Description
    {
    public:
        virtual ~Description() = default;
        virtual QString id() const = 0;
        virtual Scope scope() const = 0;
    };

    ResourceMonitor(std::unique_ptr<Description> resource, ValueGroupMonitors monitors);

    QString id() const { return m_resource->id(); }

    api::metrics::ResourceValues current(Scope requestScope) const;
    std::vector<api::metrics::Alarm> alarms(Scope requestScope) const;

    void setRules(const api::metrics::ResourceRules& rules);

private:
    const std::unique_ptr<Description> m_resource;
    ValueGroupMonitors m_monitors;
};

} // namespace nx::vms::utils::metrics
