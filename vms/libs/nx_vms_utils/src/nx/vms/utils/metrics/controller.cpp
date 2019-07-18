#include "controller.h"

#include <stdexcept>
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

api::metrics::SystemManifest Controller::manifest(RequestFlags flags) const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    api::metrics::SystemManifest systemManifest;
    for (const auto& [name, provider]: m_resourceProviders)
    {
        auto& groupManifest = systemManifest[name];
        groupManifest = provider->manifest();

        const auto groupRules = m_rules.find(name);
        if (groupRules != m_rules.end() && (flags & applyRules))
            applyRulesUnlocked(&groupManifest, groupRules->second);
    }

    return systemManifest;
}

api::metrics::SystemValues Controller::values(
    RequestFlags flags, std::optional<std::chrono::milliseconds> timeline) const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    api::metrics::SystemValues systemValues;
    const auto currentSecsSinceEpoch = m_currentSecsSinceEpoch();
    for (const auto& [name, provider]: m_resourceProviders)
    {
        auto& groupValues = systemValues[name];
        if (timeline)
        {
            groupValues = provider->timeline(
                flags & includeRemote, currentSecsSinceEpoch, *timeline);
            continue;
        }

        groupValues = provider->values(flags & includeRemote);
        if (!(flags & applyRules))
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

template<typename... Args>
std::domain_error error(Args... args)
{
    return std::domain_error(nx::utils::log::makeMessage(std::forward<Args>(args)...).toStdString());
}

static Value calculate(DataBase::Reader reader, const QStringList& formula)
{
    if (formula.isEmpty())
        throw error("no function name");

    const auto function = formula[0];
    const auto arg =
        [&](int index)
        {
            if (index >= formula.size())
                throw error("%1 arg is missing", index);

            const auto name = formula[index];
            if (const auto argReader = reader[name])
                return argReader;

            throw error("parameter %1 is not is DB", name);
        };

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

    throw error("unknown function %1", function);
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
        {
            try
            {
                parameter.value = calculate(reader, rule.calculate.split(" "));
            }
            catch(const std::domain_error& error)
            {
                parameter.value = QString(error.what());
                parameter.status = QString("error");
                continue;
            }
        }

        for (const auto& [level, alarmValue]: rule.alarms)
        {
            if (isAlarmed(parameter.value, alarmValue))
            {
                parameter.status = level;
                continue;
            }
        }
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
