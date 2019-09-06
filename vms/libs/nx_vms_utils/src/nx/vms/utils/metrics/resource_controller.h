#pragma once

#include "resource_provider.h"

namespace nx::vms::utils::metrics {

/**
 * Controlls resources of a single type.
 */
class NX_VMS_UTILS_API ResourceController
{
public:
    ResourceController(QString label): m_label(std::move(label)) {}
    virtual ~ResourceController() = default;

    const QString& label() const { return m_label; }

    virtual void start() = 0;
    virtual api::metrics::ResourceManifest manifest() const = 0;

    api::metrics::ResourceGroupValues values() const;
    std::vector<api::metrics::Alarm> alarms() const;

    api::metrics::ResourceRules rules() const;
    void setRules(api::metrics::ResourceRules rules);

protected:
    void add(const QString& id, std::unique_ptr<ResourceMonitor> monitor);
    void remove(const QString& id);

private:
    const QString m_label;
    mutable nx::utils::Mutex m_mutex;
    api::metrics::ResourceRules m_rules;
    std::map<QString /*id*/, std::unique_ptr<ResourceMonitor>> m_monitors;
};

} // namespace nx::vms::utils::metrics
