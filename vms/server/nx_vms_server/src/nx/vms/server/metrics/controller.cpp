#include "controller.h"

namespace nx::vms::server::metrics {

ResourceGroupController::ResourceGroupController(
    std::unique_ptr<AbstractResourceProvider> resourceProvider)
:
    m_resourceProvider(std::move(resourceProvider)),
    m_parameterProviders(m_resourceProvider->parameters()),
    m_parameterManifest(m_parameterProviders->manifest())
{
}

QString ResourceGroupController::id() const
{
    return m_parameterManifest.id;
}

void ResourceGroupController::startMonitoring()
{
    m_resourceProvider->startDiscovery(
        [this](QnResourcePtr resource)
        {
            AbstractParameterProvider::Monitor* monitor = nullptr;
            {
                NX_MUTEX_LOCKER locker(&m_mutex);
                const auto [iterator, isInserted] = m_parameterMonitors.emplace(
                    resource, m_parameterProviders->monitor(resource));

                monitor = iterator->second.get();
                if (!NX_ASSERT(isInserted, lm("Duplicate %1").arg(resource)))
                    return;
            }

            auto key = DataBaseKey{resource->getId().toSimpleString()};
            monitor->start(DataBaseInserter(&m_dataBase, std::move(key)));
        },
        [this](QnResourcePtr resource)
        {
            NX_MUTEX_LOCKER locker(&m_mutex);
            m_parameterMonitors.erase(resource);
        });
}

api::metrics::ResourceRules ResourceGroupController::rules()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_rules;
}

void ResourceGroupController::setRules(api::metrics::ResourceRules rules)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_rules = rules;
}

api::metrics::ResourceManifest ResourceGroupController::manifest()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    auto result = m_parameterManifest.group;
    applyRulesUnlocked(&result, m_rules);
    return result;
}

api::metrics::ResourceGroupValues ResourceGroupController::rawValues()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return rawValuesUnlocked();
}

api::metrics::ResourceGroupValues ResourceGroupController::values()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    auto result = rawValuesUnlocked();
    for (const auto& [resource, monitor]: m_parameterMonitors)
    {
        const auto resourceId = resource->getId().toSimpleString();
        applyRulesUnlocked(&result[resourceId].values, DataBaseKey{resourceId}, m_rules);
    }
    return result;
}

api::metrics::ResourceGroupValues ResourceGroupController::rawValuesUnlocked()
{
    api::metrics::ResourceGroupValues values;
    for (const auto& [resource, monitor]: m_parameterMonitors)
    {
        const auto resourceId = resource->getId().toSimpleString();
        auto& resourceValues = values[resourceId];
        resourceValues.name = resource->getName();
        resourceValues.parent = resource->getParentId().toSimpleString();
        loadGroupValuesUnlocked(
            &resourceValues.values, DataBaseKey{resourceId}, m_parameterManifest.group);
    }
    return values;
}

void ResourceGroupController::loadGroupValuesUnlocked(
    std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
    const DataBaseKey& baseKey,
    const std::vector<api::metrics::ParameterGroupManifest>& manifests)
{
    for (const auto& manifest: manifests)
    {
        auto& parameter = (*group)[manifest.id];
        const auto key = baseKey[manifest.id];

        if (manifest.group.empty())
            parameter.value = m_dataBase.value(key)->current();
        else
            loadGroupValuesUnlocked(&parameter.group, key, manifest.group);
    }
}

void ResourceGroupController::applyRulesUnlocked(
    std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
    const DataBaseKey& baseKey,
    const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules)
{
    for (const auto& [id, rule]: rules)
    {
        auto& parameter = (*group)[id];
        const auto key = baseKey[id];

        if (!parameter.group.empty())
        {
            applyRulesUnlocked(&parameter.group, key, rule.group);
            continue;
        }

        if (!rule.calculate.isEmpty())
        {
            // TODO: Add parameters calculation.
            parameter.value = "CALCULATED";
        }

        for (const auto& [level, value]: rule.alarms)
        {
            // TODO: Add checks by type.
            if (parameter.value == value)
                parameter.status = level;
        }
    }
}

void ResourceGroupController::applyRulesUnlocked(
    std::vector<api::metrics::ParameterGroupManifest>* group,
    const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules)
{
    for (const auto& [id, rule]: rules)
    {
        if (!rule.name.isEmpty())
        {
            // TODO: Locate position according to insert field.
            group->push_back(api::metrics::makeParameterManifest(id, rule.name));
            continue;
        }

        if (!rule.group.empty())
        {
            auto it = std::find_if(
                group->begin(), group->end(), [&](auto item) { return item.id == id; });

            if (!NX_ASSERT(it != group->end()))
                continue;

            applyRulesUnlocked(&it->group, rule.group);
            continue;
        }
    }
}

} // namespace nx::vms::server::metrics
