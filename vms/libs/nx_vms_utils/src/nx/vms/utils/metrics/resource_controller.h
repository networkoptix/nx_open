// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_provider.h"

namespace nx::vms::utils::metrics {

/**
 * Controlls resources of a single type.
 */
class NX_VMS_UTILS_API ResourceController
{
public:
    ResourceController(QString name): m_name(std::move(name)) {}
    virtual ~ResourceController() = default;

    const QString& name() const { return m_name; }

    virtual void start() = 0;
    virtual api::metrics::ResourceManifest manifest() const = 0;

    api::metrics::ResourceGroupValues values(Scope requestScope, bool formatted);
    api::metrics::ResourceGroupAlarms alarms(Scope requestScope);

    std::pair<api::metrics::ResourceGroupValues, Scope> valuesWithScope(Scope requestScope,
        bool formatted,
        const nx::utils::DotNotationString& filter = {});
    std::pair<api::metrics::ResourceGroupAlarms, Scope> alarmsWithScope(Scope requestScope,
        const nx::utils::DotNotationString& filter = {});

    api::metrics::ResourceRules rules() const;
    void setRules(api::metrics::ResourceRules rules);

protected:
    void add(std::unique_ptr<ResourceMonitor> monitor);
    bool remove(const QString& id);

    // NOTE: Those functions are called mutex-free.
    virtual void beforeValues(Scope /*scope*/, bool /*format*/) {}
    virtual void beforeAlarms(Scope /*scope*/) {}

private:
    const QString m_name;
    mutable nx::Mutex m_mutex;
    api::metrics::ResourceRules m_rules;
    std::map<QString /*id*/, std::unique_ptr<ResourceMonitor>> m_monitors;
};

} // namespace nx::vms::utils::metrics
