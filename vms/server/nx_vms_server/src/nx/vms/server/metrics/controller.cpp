#include "controller.h"

namespace nx::vms::server::metrics {

void Controller::registerGroup(std::unique_ptr<AbstractResourceProvider> resourceProvider)
{
    ResourceGroup group;
    group.resourceProvider = std::move(resourceProvider);
    group.parameterProviders = group.resourceProvider->parameters();
    group.parameterManifest = group.parameterProviders->manifest();

    const auto id = group.parameterManifest.id;
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_resourceGroups[id] = std::move(group);
}

void Controller::startMonitoring()
{
    for (auto& [id, group]: m_resourceGroups)
    {
        group.resourceProvider->startDiscovery(
            [this, &group](QnResourcePtr resource)
            {
                AbstractParameterProvider::Monitor* monitor = nullptr;
                {
                    NX_MUTEX_LOCKER locker(&m_mutex);
                    const auto [iterator, isInserted] = group.parameterMonitors.emplace(
                        resource, group.parameterProviders->monitor(resource));

                    monitor = iterator->second.get();
                    if (!NX_ASSERT(isInserted, lm("Duplicate %1").arg(resource)))
                        return;
                }

                monitor->start(m_dataBase.access(resource->getId().toSimpleString()));
            },
            [this, &group](QnResourcePtr resource)
            {
                NX_MUTEX_LOCKER locker(&m_mutex);
                group.parameterMonitors.erase(resource);
            });
    }
}

api::metrics::SystemRules Controller::rules()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_rules;
}

void Controller::setRules(api::metrics::SystemRules rules)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_rules = rules;
}

api::metrics::SystemManifest Controller::manifest()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    api::metrics::SystemManifest systemManifest;
    for (const auto& [groupId, group]: m_resourceGroups)
    {
        auto groupManifest = group.parameterManifest.group;
        applyRulesUnlocked(&groupManifest, m_rules[groupId]);
        systemManifest[groupId] =  std::move(groupManifest);
    }

    return systemManifest;
}

api::metrics::SystemValues Controller::rawValues()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return rawValuesUnlocked();
}

api::metrics::SystemValues Controller::values()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    auto systemValues = rawValuesUnlocked();
    for (const auto& [groupId, group]: m_resourceGroups)
    {
        auto& groupValues = systemValues[groupId];
        for (const auto& [resource, monitor]: group.parameterMonitors)
        {
            const auto resourceId = resource->getId().toSimpleString();
            applyRulesUnlocked(
                &groupValues[resourceId].values,
                m_dataBase.access(resourceId),
                m_rules[groupId]);
        }
    }

    return systemValues;
}

api::metrics::SystemValues Controller::rawValuesUnlocked()
{
    api::metrics::SystemValues systemValues;
    for (const auto& [groupId, group]: m_resourceGroups)
    {
        auto& groupValues = systemValues[groupId];
        for (const auto& [resource, monitor]: group.parameterMonitors)
        {
            const auto resourceId = resource->getId().toSimpleString();

            auto& resourceValues = groupValues[resourceId];
            resourceValues.name = resource->getName();
            resourceValues.parent = resource->getParentId().toSimpleString();

            loadGroupValuesUnlocked(
                &resourceValues.values,
                m_dataBase.access(resourceId),
                group.parameterManifest.group);
        }
    }

    return systemValues;
}

void Controller::loadGroupValuesUnlocked(
    std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
    DataBase::Access groupAccess,
    const std::vector<api::metrics::ParameterGroupManifest>& manifests)
{
    for (const auto& manifest: manifests)
    {
        auto& parameter = (*group)[manifest.id];
        const auto access = groupAccess[manifest.id];

        if (manifest.group.empty())
            parameter.value = access->current();
        else
            loadGroupValuesUnlocked(&parameter.group, access, manifest.group);
    }
}

static api::metrics::Value calculate(DataBase::Access access, const QStringList& formula)
{
    if (formula[0] == "count")
    {
        const auto values = access[formula[1]]->last().size();
        return static_cast<double>(values ? values - 1 : 0);
    }

    return "ERROR: unknown function: " + formula[0];
}

static api::metrics::Status checkStatus(
    const api::metrics::Value& value,
    const std::map<api::metrics::Status, api::metrics::Value>& alarmRules)
{
    for (const auto& [level, alarmValue]: alarmRules)
    {
        if (value.type() == api::metrics::Value::Double && value.toDouble() >= alarmValue.toDouble())
            return level;

        if (value == alarmValue)
            return level;
    }

    return {};
}

void Controller::applyRulesUnlocked(
    std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
    DataBase::Access groupAccess,
    const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules)
{
    for (const auto& [id, rule]: rules)
    {
        auto& parameter = (*group)[id];
        if (!parameter.group.empty())
        {
            applyRulesUnlocked(&parameter.group, groupAccess[id], rule.group);
            continue;
        }

        if (!rule.calculate.isEmpty())
            parameter.value = calculate(groupAccess, rule.calculate.split(" "));

        parameter.status = checkStatus(parameter.value, rule.alarms);
    }
}

static std::vector<api::metrics::ParameterGroupManifest>::iterator insertPosition(
    std::vector<api::metrics::ParameterGroupManifest>& group,
    const QStringList& desire)
{
    auto position = api::metrics::find(group, desire[1]);
    if (position != group.end() && desire[0] == "after")
        return position + 1;
    return position;
}

void Controller::applyRulesUnlocked(
    std::vector<api::metrics::ParameterGroupManifest>* group,
    const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules)
{
    for (const auto& [id, rule]: rules)
    {
        if (!rule.name.isEmpty())
        {
            group->insert(
                insertPosition(*group, rule.insert.split(" ")),
                api::metrics::makeParameterManifest(id, rule.name));
            continue;
        }

        if (!rule.group.empty())
        {
            auto it = api::metrics::find(*group, id);
            if (!NX_ASSERT(it != group->end()))
                continue;

            applyRulesUnlocked(&it->group, rule.group);
            continue;
        }
    }
}

} // namespace nx::vms::server::metrics
