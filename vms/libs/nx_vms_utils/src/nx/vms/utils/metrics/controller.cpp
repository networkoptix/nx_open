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
        provider->startMonitoring(m_dataBase.writer(name));
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
    bool applyRules, std::optional<std::chrono::milliseconds> timeline) const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    api::metrics::SystemValues systemValues;
    for (const auto& [name, provider]: m_resourceProviders)
    {
        auto& groupValues = systemValues[name];
        if (timeline)
        {
            groupValues = provider->timeline(m_currentSecsSinceEpoch(), *timeline);
            continue;
        }

        groupValues = provider->values();
        if (!applyRules)
            continue;

        const auto groupRules = m_rules.find(name);
        if (groupRules == m_rules.end())
            continue;

        const auto groupDb = m_dataBase.reader(name);
        for (auto& [resourceId, resourceValues]: groupValues)
            applyRulesUnlocked(&resourceValues.values, groupDb[resourceId], groupRules->second);
    }

    return systemValues;
}

static double sum(const std::vector<nx::utils::TimedValue<Value>>& timeline)
{
    return std::accumulate(
        timeline.begin(), timeline.end(), (double) 0,
        [](double current, const auto& point) { return current + point.value.toDouble(); });
}

static Value calculate(DataBase::Reader reader, const QStringList& formula)
{
    if (formula.isEmpty())
        return "ERROR: No function";

    const auto function = formula[0];
    const auto arg =
        [&](int index) { return reader[index <= formula.size() ? formula[index] : QString()]; };

    if (function == "+" || formula[0] == "add")
        return arg(1)->current().toDouble() + arg(2)->current().toDouble();

    if (function == "-" || formula[0] == "sub")
        return arg(1)->current().toDouble() - arg(2)->current().toDouble();

    if (function == "count")
    {
        const auto count = arg(1)->last().size();
        return static_cast<double>(count ? count - 1 : 0);
    }

    if (function == "sum")
        return sum(arg(1)->last());

    if (function == "avg" || function == "average")
    {
        const auto timeline = arg(1)->last();
        return timeline.empty() ? 0 : (sum(timeline) / timeline.size());
    }

    return "ERROR: unknown function: " + function;
}

static bool isAlarmed(const Value& value, const Value& alarmValue)
{
    if (alarmValue.type() == Value::Double)
        return value.toDouble() >= alarmValue.toDouble();

    if (alarmValue.type() == Value::String)
    {
        const auto string = alarmValue.toString();
        const auto expectOprator =
            [string](const QString& expectedOperator) -> std::optional<QString>
            {
                if (string.startsWith(expectedOperator))
                    return string.mid(expectedOperator.size());

                return std::nullopt;
            };

        if (const auto expectedValue = expectOprator("="))
            return value.toString() == *expectedValue;

        if (const auto expectedValue = expectOprator("!="))
            return value.toString() != *expectedValue;

        if (const auto expectedValue = expectOprator(">="))
            return value.toDouble() >= expectedValue->toDouble();

        if (const auto expectedValue = expectOprator(">"))
            return value.toDouble() > expectedValue->toDouble();

        if (const auto expectedValue = expectOprator("<="))
            return value.toDouble() <= expectedValue->toDouble();

        if (const auto expectedValue = expectOprator("<"))
            return value.toDouble() < expectedValue->toDouble();
    }

    return value == alarmValue;
}

static api::metrics::Status checkStatus(
    const Value& value, const std::map<api::metrics::Status, Value>& alarmRules)
{
    if (value.isNull() || value.toString().startsWith("ERROR: "))
        return "error";

    for (const auto& [level, alarmValue]: alarmRules)
    {
        if (isAlarmed(value, alarmValue))
            return level;
    }

    return {};
}

void Controller::applyRulesUnlocked(
    std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
    DataBase::Reader reader,
    const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules) const
{
    for (const auto& [id, rule]: rules)
    {
        auto& parameter = (*group)[id];
        if (!parameter.group.empty())
        {
            applyRulesUnlocked(&parameter.group, reader[id], rule.group);
            continue;
        }

        if (!rule.calculate.isEmpty())
            parameter.value = calculate(reader, rule.calculate.split(" "));

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
