#include "controller.h"

namespace nx::vms::utils::metrics {

void Controller::registerGroup(QString name, std::unique_ptr<AbstractResourceProvider> resourceProvider)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_resourceProviders[name] = std::move(resourceProvider);
}

void Controller::startMonitoring()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    for (const auto& [name, provider]: m_resourceProviders)
        provider->startMonitoring(m_dataBase.access(name));
}

api::metrics::SystemRules Controller::rules() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_rules;
}

void Controller::setRules(api::metrics::SystemRules rules)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_rules = rules;
}

api::metrics::SystemManifest Controller::manifest(bool applyRules) const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    api::metrics::SystemManifest systemManifest;
    for (const auto& [name, provider]: m_resourceProviders)
    {
        auto& groupManifest = systemManifest[name];
        groupManifest = provider->manifest();

        const auto groupRules = m_rules.find(name);
        if (groupRules != m_rules.end() && applyRules)
            applyRulesUnlocked(&groupManifest, groupRules->second);
    }

    return systemManifest;
}

api::metrics::SystemValues Controller::values(bool applyRules) const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    api::metrics::SystemValues systemValues;
    for (const auto& [name, provider]: m_resourceProviders)
    {
        auto& groupValues = systemValues[name];
        const auto groupDb = m_dataBase.access(name);
        const auto groupRules = m_rules.find(name);
        for (const auto& resource: provider->resources())
        {
            auto& resourceValues = groupValues[resource.id];
            resourceValues.name = resource.name;
            resourceValues.parent = resource.parent;

            const auto resourceDb = groupDb[resource.id];
            loadGroupValuesUnlocked(&resourceValues.values, resourceDb, provider->manifest());

            if (groupRules != m_rules.end() && applyRules)
                applyRulesUnlocked(&resourceValues.values, resourceDb, groupRules->second);
        }
    }

    return systemValues;
}

void Controller::loadGroupValuesUnlocked(
    std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
    DataBase::Access groupAccess,
    const std::vector<api::metrics::ParameterGroupManifest>& manifests) const
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

static Value calculate(DataBase::Access access, const QStringList& formula)
{
    if (formula[0] == "count")
    {
        const auto values = access[formula[1]]->last().size();
        return static_cast<double>(values ? values - 1 : 0);
    }

    return "ERROR: unknown function: " + formula[0];
}

static api::metrics::Status checkStatus(
    const Value& value, const std::map<api::metrics::Status, Value>& alarmRules)
{
    for (const auto& [level, alarmValue]: alarmRules)
    {
        if (value.type() == Value::Double && value.toDouble() >= alarmValue.toDouble())
            return level;

        if (value == alarmValue)
            return level;
    }

    return {};
}

void Controller::applyRulesUnlocked(
    std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
    DataBase::Access groupAccess,
    const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules) const
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
    const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules) const
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

} // namespace nx::vms::utils::metrics
