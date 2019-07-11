#pragma once

#include <nx/utils/scope_guard.h>

#include "data_base.h"

namespace nx::vms::utils::metrics {

using Values = api::metrics::ParameterGroupValues;

template<typename ResourceType>
using Getter = nx::utils::MoveOnlyFunc<Value(const ResourceType&)>;

using OnChange = nx::utils::MoveOnlyFunc<void()>;

template<typename ResourceType>
using Watch = nx::utils::MoveOnlyFunc<nx::utils::SharedGuardPtr(const ResourceType&, OnChange)>;

/**
 * Allows to monitor parameter value.
 */
class NX_VMS_UTILS_API AbstractParameterMonitor
{
public:
    virtual ~AbstractParameterMonitor() = default;

    virtual Values current() = 0;
    virtual std::optional<Values> timeline(
        uint64_t nowSecsSinceEpoch, std::chrono::milliseconds length);
};

using ParameterMonitorPtr = std::unique_ptr<AbstractParameterMonitor>;

/**
 * Provides a value at any given time only.
 */
template<typename ResourceType>
class RuntimeParameterMonitor: public AbstractParameterMonitor
{
public:
    RuntimeParameterMonitor(const ResourceType& resource, const Getter<ResourceType>& getter);

    Values current() override;

protected:
    const ResourceType m_resource;
    const Getter<ResourceType>& m_getter;
};

/**
 * Provides a value at any given time as well as all of the changes history.
 */
template<typename ResourceType>
class DataBaseParameterMonitor: public RuntimeParameterMonitor<ResourceType>
{
public:
    DataBaseParameterMonitor(
        const ResourceType& resource,
        const Getter<ResourceType>& getter,
        DataBase::Writer dbWriter,
        const Watch<ResourceType>& watch);

    Values current() override;
    std::optional<Values> timeline(
        uint64_t nowSecsSinceEpoch, std::chrono::milliseconds length) override;

private:
    void updateValue();

private:
    const DataBase::Writer m_dbWriter;
    nx::utils::SharedGuardPtr m_watchGuard;
};

/**
 * Provides values for parameter group.
 */
template<typename ResourceType>
class ParameterGroupMonitor: public AbstractParameterMonitor
{
public:
    ParameterGroupMonitor(std::map<QString, ParameterMonitorPtr> monitors);

    Values current() override;
    std::optional<Values> timeline(
        uint64_t nowSecsSinceEpoch, std::chrono::milliseconds length) override;

private:
    std::map<QString, ParameterMonitorPtr> m_monitors;
};

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
RuntimeParameterMonitor<ResourceType>::RuntimeParameterMonitor(
    const ResourceType& resource,
    const Getter<ResourceType>& getter)
:
    m_resource(resource),
    m_getter(getter)
{
}

template<typename ResourceType>
Values RuntimeParameterMonitor<ResourceType>::current()
{
    return api::metrics::makeParameterValue(m_getter(m_resource));
}

template<typename ResourceType>
DataBaseParameterMonitor<ResourceType>::DataBaseParameterMonitor(
    const ResourceType& resource,
    const Getter<ResourceType>& getter,
    DataBase::Writer dbWriter,
    const Watch<ResourceType>& watch)
:
    RuntimeParameterMonitor<ResourceType>(resource, std::move(getter)),
    m_dbWriter(dbWriter),
    m_watchGuard(watch(resource, [this](){ updateValue(); }))
{
    updateValue();
}

template<typename ResourceType>
Values DataBaseParameterMonitor<ResourceType>::current()
{
    return api::metrics::makeParameterValue(m_dbWriter->current());
}

template<typename ResourceType>
std::optional<Values> DataBaseParameterMonitor<ResourceType>::timeline(
    uint64_t nowSecsSinceEpoch, std::chrono::milliseconds length)
{
    const auto timedValues = m_dbWriter->last(length);
    const auto now = nx::utils::monotonicTime();
    QJsonObject values;
    for (const auto& [value, time]: timedValues)
    {
        const auto delayS = std::chrono::duration_cast<std::chrono::seconds>(now - time).count();
        values[QString::number(nowSecsSinceEpoch - delayS)] = value;
    }

    return api::metrics::makeParameterValue(values);
}

template<typename ResourceType>
void DataBaseParameterMonitor<ResourceType>::updateValue()
{
    return m_dbWriter->update(this->m_getter(this->m_resource));
}

template<typename ResourceType>
ParameterGroupMonitor<ResourceType>::ParameterGroupMonitor(
    std::map<QString, ParameterMonitorPtr> monitors)
:
    m_monitors(std::move(monitors))
{
}

template<typename ResourceType>
Values ParameterGroupMonitor<ResourceType>::current()
{
    Values values;
    for (const auto& [id, monior]: m_monitors)
        values.group[id] = monior->current();

    return values;
}

template<typename ResourceType>
std::optional<Values> ParameterGroupMonitor<ResourceType>::timeline(
    uint64_t nowSecsSinceEpoch, std::chrono::milliseconds length)
{
    Values values;
    for (const auto& [id, monitor]: m_monitors)
    {
        if (auto value = monitor->timeline(nowSecsSinceEpoch, length))
            values.group[id] = std::move(*value);
    }

    if (values.group.empty())
        return std::nullopt;

    return values;
}

} // namespace nx::vms::utils::metrics
