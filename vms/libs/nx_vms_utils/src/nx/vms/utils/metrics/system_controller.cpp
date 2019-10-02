#include "system_controller.h"

#include <nx/utils/log/log.h>

namespace nx::vms::utils::metrics {

void SystemController::add(std::unique_ptr<ResourceController> resourceController)
{
    const auto label = resourceController->label();
    const auto [it, isOk] = m_resourceControllers.emplace(label, std::move(resourceController));
    NX_ASSERT(isOk, "Label duplicate: %1", label);
    NX_DEBUG(this, "Added [%1] controller: %2", label, resourceController);
}

void SystemController::start()
{
    NX_VERBOSE(this, "Starting with %1 resource controllers", m_resourceControllers.size());
    for (const auto& [label, controller]: m_resourceControllers)
    {
        (void) label;
        controller->start();
    }
}

api::metrics::SystemManifest SystemController::manifest() const
{
    const auto start = std::chrono::steady_clock::now();
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_manifestCache)
    {
        m_manifestCache = std::make_unique<api::metrics::SystemManifest>();
        for (const auto& [label, controller]: m_resourceControllers)
            (*m_manifestCache)[label] = controller->manifest();
    }

    NX_DEBUG(this, "Return manifest in %1", std::chrono::steady_clock::now() - start);
    return *m_manifestCache;
}

api::metrics::SystemValues SystemController::values(Scope requestScope, bool formatted) const
{
    const auto start = std::chrono::steady_clock::now();
    api::metrics::SystemValues systemValues;
    for (const auto& [label, controller]: m_resourceControllers)
    {
        if (auto values = controller->values(requestScope, formatted); !values.empty())
            systemValues[label] = std::move(values);
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

std::vector<api::metrics::Alarm> SystemController::alarms(Scope requestScope) const
{
    const auto start = std::chrono::steady_clock::now();
    std::vector<api::metrics::Alarm> allAlarms;
    for (const auto& [label, controller]: m_resourceControllers)
    {
        auto alarms = controller->alarms(requestScope);
        for (auto& alarm: alarms)
        {
            (void) label; // TODO: Add label?
            allAlarms.push_back(std::move(alarm));
        }
    }

    NX_DEBUG(this, "Return %1 %2 alarm(s) in %3",
        allAlarms.size(), requestScope, std::chrono::steady_clock::now() - start);
    return allAlarms;
}

api::metrics::SystemRules SystemController::rules() const
{
    api::metrics::SystemRules rules;
    for (const auto& [label, controller]: m_resourceControllers)
        rules[label] = controller->rules();

    return rules;
}

void SystemController::setRules(api::metrics::SystemRules rules)
{
    for (auto& [label, resourceRules]: rules)
    {
        const auto it = m_resourceControllers.find(label);
        if (!NX_ASSERT(it != m_resourceControllers.end(), "Unexpected rules: %1", label))
            return;

        it->second->setRules(std::move(resourceRules));
    }

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_manifestCache.reset();
}

} // namespace nx::vms::utils::metrics
