// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_controller.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx::vms::utils::metrics {

SystemController::~SystemController()
{
    NX_DEBUG(this, "Removed");
}

void SystemController::add(std::unique_ptr<ResourceController> resourceController)
{
    const auto duplicate = std::find_if(
        m_resourceControllers.begin(), m_resourceControllers.end(),
        [&resourceController](const auto& c) { return c->name() == resourceController->name(); });

    NX_ASSERT(duplicate == m_resourceControllers.end(), "Label duplicate with %1", *duplicate);
    NX_DEBUG(this, "Add %1 as %2", resourceController, resourceController->name());
    m_resourceControllers.push_back(std::move(resourceController));
}

void SystemController::start()
{
    NX_DEBUG(this, "Starting with %1 resource controllers", m_resourceControllers.size());
    for (const auto& controller: m_resourceControllers)
        controller->start();
}

api::metrics::SiteManifest SystemController::manifest() const
{
    const auto start = std::chrono::steady_clock::now();
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_manifestCache)
    {
        m_manifestCache = std::make_unique<api::metrics::SiteManifest>();
        for (const auto& controller: m_resourceControllers)
            m_manifestCache->push_back(controller->manifest());
    }

    NX_DEBUG(this, "Return manifest in %1", std::chrono::steady_clock::now() - start);
    return *m_manifestCache;
}

api::metrics::SiteValues SystemController::values(Scope requestScope, bool formatted) const
{
    return valuesWithScope(requestScope, formatted).first;
}

std::pair<api::metrics::SiteValues, Scope> SystemController::valuesWithScope(Scope requestScope,
    bool formatted,
    const nx::utils::DotNotationString& filter) const
{
    const auto start = std::chrono::steady_clock::now();
    auto actualScope = requestScope;
    api::metrics::SiteValues systemValues;
    for (const auto& controller: m_resourceControllers)
    {
        if (!filter.accepts(controller->name()))
            continue;

        nx::utils::DotNotationString nested;
        if (auto it = filter.findWithWildcard(controller->name()); it != filter.end())
            nested = it.value();

        if (auto values = controller->valuesWithScope(requestScope, formatted, std::move(nested)); !values.first.empty())
        {
            systemValues[controller->name()] = std::move(values.first);

            if (values.second == Scope::local)
                actualScope = Scope::local;
        }
    }

    const auto counts =
        [&]()
        {
            QStringList list;
            for (const auto& [label, values]: systemValues)
                list << nx::format("%1 %2").args(values.size(), label);
            return list;
        };

    NX_DEBUG(this, "Return %1 from %2 values in %3",
        counts().join(", "), requestScope, std::chrono::steady_clock::now() - start);

    return {systemValues, actualScope};
}

api::metrics::SiteAlarms SystemController::alarms(Scope requestScope) const
{
    return alarmsWithScope(requestScope).first;
}

std::pair<api::metrics::SiteAlarms, Scope> SystemController::alarmsWithScope(Scope requestScope,
    const nx::utils::DotNotationString& filter) const
{
    auto actualScope = requestScope;
    api::metrics::SiteAlarms systemAlarms;
    for (const auto& controller: m_resourceControllers)
    {
        if (!filter.accepts(controller->name()))
            continue;

        nx::utils::DotNotationString nested;
        if (auto it = filter.findWithWildcard(controller->name()); it != filter.end())
            nested = it.value();

        if (auto alarms = controller->alarmsWithScope(requestScope, std::move(nested)); !alarms.first.empty())
        {
            systemAlarms[controller->name()] = std::move(alarms.first);

            if (alarms.second == Scope::local)
                actualScope = Scope::local;
        }
    }

    NX_DEBUG(this, "Return %1 %2 alarmed resources", systemAlarms.size(), requestScope);
    return {systemAlarms, actualScope};
}

api::metrics::SiteRules SystemController::rules() const
{
    api::metrics::SiteRules rules;
    for (const auto& controller: m_resourceControllers)
        rules[controller->name()] = controller->rules();

    return rules;
}

void SystemController::setRules(api::metrics::SiteRules rules, bool makePermament)
{
    NX_ASSERT(!m_areRulesPermanent, "Rules change was forbidden");
    if (makePermament)
        m_areRulesPermanent = true;

    for (const auto& controller: m_resourceControllers)
    {
        const auto it = rules.find(controller->name());
        if (it == rules.end())
            continue;

        controller->setRules(std::move(it->second));
        rules.erase(it);
    }

    NX_ASSERT(rules.empty(), "Unused rules: %1", QJson::serialized<api::metrics::SiteRules>(rules));
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_manifestCache.reset();
}

} // namespace nx::vms::utils::metrics
