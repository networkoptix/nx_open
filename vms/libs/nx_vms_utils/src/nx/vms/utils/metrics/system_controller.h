// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_controller.h"

namespace nx::vms::utils::metrics {

/**
 * Controls all of the resources in the system.
 */
class NX_VMS_UTILS_API SystemController
{
public:
    ~SystemController();
    void add(std::unique_ptr<ResourceController> resourceController);
    void start();

    api::metrics::SiteManifest manifest() const;
    api::metrics::SiteValues values(Scope requestScope, bool formatted) const;
    api::metrics::SiteAlarms alarms(Scope requestScope) const;

    std::pair<api::metrics::SiteValues, Scope> valuesWithScope(Scope requestScope,
        bool formatted,
        const nx::utils::DotNotationString& filter = {}) const;
    std::pair<api::metrics::SiteAlarms, Scope> alarmsWithScope(Scope requestScope,
        const nx::utils::DotNotationString& filter = {}) const;

    api::metrics::SiteRules rules() const;
    void setRules(api::metrics::SiteRules rules, bool makePermament = false);

private:
    std::atomic<bool> m_areRulesPermanent = false;
    std::vector<std::unique_ptr<ResourceController>> m_resourceControllers;

    mutable nx::Mutex m_mutex;
    mutable std::unique_ptr<api::metrics::SiteManifest> m_manifestCache;
};

} // namespace nx::vms::utils::metrics
