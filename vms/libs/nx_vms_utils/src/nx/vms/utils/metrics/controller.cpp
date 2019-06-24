#include "controller.h"

#include <QtCore/QJsonObject>

namespace nx::vms::utils::metrics {

static uint64_t pcSecsSinceEpoch()
{
    return (uint64_t) nx::utils::timeSinceEpoch().count();
}

Controller::Controller(nx::utils::MoveOnlyFunc<uint64_t()> currentSecsSinceEpoch):
    m_currentSecsSinceEpoch(
        currentSecsSinceEpoch ? std::move(currentSecsSinceEpoch) : &pcSecsSinceEpoch)
{
}

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

api::metrics::SystemValues Controller::values(
    bool applyRules, std::optional<std::chrono::milliseconds> timeLine) const
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
            loadGroupValuesUnlocked(
                &resourceValues.values, resourceDb, provider->manifest(), timeLine);

            if (groupRules != m_rules.end() && applyRules && !timeLine)
                applyRulesUnlocked(&resourceValues.values, resourceDb, groupRules->second);
        }
    }

    return systemValues;
}

void Controller::loadGroupValuesUnlocked(
    std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
    DataBase::Access groupAccess,
    const std::vector<api::metrics::ParameterGroupManifest>& manifests,
    std::optional<std::chrono::milliseconds> timeLine) const
{
    for (const auto& manifest: manifests)
    {
        auto& parameter = (*group)[manifest.id];
        const auto access = groupAccess[manifest.id];

        if (manifest.group.empty())
            parameter.value = timeLine ? makeTimeLine(access, *timeLine) : access->current();
        else
            loadGroupValuesUnlocked(&parameter.group, access, manifest.group, timeLine);
    }
}

Value Controller::makeTimeLine(DataBase::Access access, std::chrono::milliseconds timeLine) const
{
    const auto timedValues = access->last(timeLine);
    const auto now = nx::utils::monotonicTime();
    const auto timeSinseEpochS = m_currentSecsSinceEpoch();
    QJsonObject values;
    for (const auto& [value, time]: timedValues)
    {
        const auto delayS = std::chrono::duration_cast<std::chrono::seconds>(now - time).count();
        values[QString::number(timeSinseEpochS - delayS)] = value;
    }

    return values;
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
