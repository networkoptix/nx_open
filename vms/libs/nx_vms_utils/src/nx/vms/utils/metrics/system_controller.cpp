#include "system_controller.h"

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::utils::metrics {

void SystemController::add(std::unique_ptr<ResourceController> resourceController)
{
    const auto duplicate = std::find_if(
        m_resourceControllers.begin(), m_resourceControllers.end(),
        [&resourceController](const auto& c) { return c->name() == resourceController->name(); });

    NX_ASSERT(duplicate == m_resourceControllers.end(), "Label duplicate with %1", *duplicate);
    m_resourceControllers.push_back(std::move(resourceController));
}

void SystemController::start()
{
    NX_VERBOSE(this, "Starting with %1 resource controllers", m_resourceControllers.size());
    for (const auto& controller: m_resourceControllers)
        controller->start();
}

api::metrics::SystemManifest SystemController::manifest() const
{
    const auto start = std::chrono::steady_clock::now();
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_manifestCache)
    {
        m_manifestCache = std::make_unique<api::metrics::SystemManifest>();
        for (const auto& controller: m_resourceControllers)
            m_manifestCache->push_back(controller->manifest());
    }

    NX_DEBUG(this, "Return manifest in %1", std::chrono::steady_clock::now() - start);
    return *m_manifestCache;
}

api::metrics::SystemValues SystemController::values(Scope requestScope, bool formatted) const
{
    const auto start = std::chrono::steady_clock::now();
    api::metrics::SystemValues systemValues;
    for (const auto& controller: m_resourceControllers)
    {
        if (auto values = controller->values(requestScope, formatted); !values.empty())
            systemValues[controller->name()] = std::move(values);
    }

    const auto counts =
        [&]()
        {
            QStringList list;
            for (const auto& [label, values]: systemValues)
                list << lm("%1 %2").args(values.size(), label);
            return list;
        };

    NX_DEBUG(this, "Return %1 from %2 values in %3",
        counts().join(", "), requestScope, std::chrono::steady_clock::now() - start);
    return systemValues;
}

api::metrics::SystemAlarms SystemController::alarms(Scope requestScope) const
{
    api::metrics::SystemAlarms systemAlarms;
    for (const auto& controller: m_resourceControllers)
    {
        if (auto alarms = controller->alarms(requestScope); !alarms.empty())
            systemAlarms[controller->name()] = std::move(alarms);
    }

    NX_DEBUG(this, "Return %1 %2 alarmed resources", systemAlarms.size(), requestScope);
    return systemAlarms;
}

api::metrics::SystemRules SystemController::rules() const
{
    api::metrics::SystemRules rules;
    for (const auto& controller: m_resourceControllers)
        rules[controller->name()] = controller->rules();

    return rules;
}

void SystemController::setRules(api::metrics::SystemRules rules)
{
    for (const auto& controller: m_resourceControllers)
    {
        const auto it = rules.find(controller->name());
        if (it == rules.end())
            continue;

        controller->setRules(std::move(it->second));
        rules.erase(it);
    }

    NX_ASSERT(rules.empty(), "Unused rules: %1", QJson::serialized<api::metrics::SystemRules>(rules));
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_manifestCache.reset();
}

} // namespace nx::vms::utils::metrics
