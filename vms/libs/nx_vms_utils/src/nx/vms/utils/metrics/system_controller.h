// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_controller.h"

namespace nx::vms::utils::metrics {

/**
 * Controlls all of the resources in the system.
 */
class NX_VMS_UTILS_API SystemController
{
public:
    ~SystemController();
    void add(std::unique_ptr<ResourceController> resourceController);
    void start();

    api::metrics::SystemManifest manifest() const;
    api::metrics::SystemValues values(Scope requestScope, bool formatted) const;
    api::metrics::SystemAlarms alarms(Scope requestScope) const;

    api::metrics::SystemRules rules() const;
    void setRules(api::metrics::SystemRules rules, bool makePermament = false);

private:
    std::atomic<bool> m_areRulesPermanent = false;
    std::vector<std::unique_ptr<ResourceController>> m_resourceControllers;

    mutable nx::Mutex m_mutex;
    mutable std::unique_ptr<api::metrics::SystemManifest> m_manifestCache;
};

} // namespace nx::vms::utils::metrics
