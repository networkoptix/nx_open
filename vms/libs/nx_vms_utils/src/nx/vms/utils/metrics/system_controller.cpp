#include "system_controller.h"

namespace nx::vms::utils::metrics {

void SystemController::add(std::unique_ptr<ResourceController> resourceController)
{
    const auto label = resourceController->label();
    const auto [it, isOk] = m_resourceControllers.emplace(label, std::move(resourceController));
    NX_ASSERT(isOk, "Label duplicate: %1", label);
}

void SystemController::start()
{
    for (const auto& [label, controller]: m_resourceControllers)
    {
        (void) label;
        controller->start();
    }
}

api::metrics::SystemManifest SystemController::manifest() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_manifestCache)
    {
        m_manifestCache = std::make_unique<api::metrics::SystemManifest>();
        for (const auto& [label, controller]: m_resourceControllers)
            (*m_manifestCache)[label] = controller->manifest();
    }

    return *m_manifestCache;
}

api::metrics::SystemValues SystemController::values() const
{
    api::metrics::SystemValues manifest;
    for (const auto& [label, controller]: m_resourceControllers)
        manifest[label] = controller->values();

    return manifest;
}

std::vector<api::metrics::Alarm> SystemController::alarms() const
{
    std::vector<api::metrics::Alarm> allAlarms;
    for (const auto& [label, controller]: m_resourceControllers)
    {
        auto alarms = controller->alarms();
        for (auto& alarm: alarms)
        {
            (void) label; // TODO: Add label?
            allAlarms.push_back(std::move(alarm));
        }
    }

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
