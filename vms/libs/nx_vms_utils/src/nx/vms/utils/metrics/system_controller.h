#pragma once

#include "resource_controller.h"

namespace nx::vms::utils::metrics {

/**
 * Controlls all of the resources in the system.
 */
class SystemController
{
public:
    void add(std::unique_ptr<ResourceController> resourceController);
    void start();

    api::metrics::SystemManifest manifest() const;
    api::metrics::SystemValues values() const;
    std::vector<api::metrics::Alarm> alarms() const;

    api::metrics::SystemRules rules() const;
    void setRules(api::metrics::SystemRules rules);

private:
    std::map<QString /*label*/, std::unique_ptr<ResourceController>> m_resourceControllers;

    mutable nx::utils::Mutex m_mutex;
    mutable std::unique_ptr<api::metrics::SystemManifest> m_manifestCache;
};

} // namespace nx::vms::utils::metrics
