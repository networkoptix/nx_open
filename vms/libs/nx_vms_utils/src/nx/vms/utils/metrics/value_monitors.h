#pragma once

#include <nx/vms/api/metrics.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/value_history.h>
#include <nx/utils/move_only_func.h>

namespace nx::vms::utils::metrics {

using Duration = std::chrono::milliseconds;
using ValueIterator = std::function<void(const api::metrics::Value& value, Duration age)>;

/**
 * Allows to monitor parameter value.
 */
class NX_VMS_UTILS_API ValueMonitor
{
public:
    virtual ~ValueMonitor() = default;

    virtual api::metrics::Value current() const = 0;
    virtual void forEach(Duration maxAge, const ValueIterator& iterator) const = 0;
};

using ValueMonitors = std::map<QString, std::unique_ptr<ValueMonitor>>;

template<typename ResourceType>
using Getter = nx::utils::MoveOnlyFunc<api::metrics::Value(const ResourceType&)>;

using OnChange = nx::utils::MoveOnlyFunc<void()>;

template<typename ResourceType>
using Watch = nx::utils::MoveOnlyFunc<nx::utils::SharedGuardPtr(const ResourceType&, OnChange)>;

/**
 * Provides a value at any given time only.
 */
template<typename ResourceType>
class RuntimeValueMonitor: public ValueMonitor
{
public:
    RuntimeValueMonitor(const ResourceType& resource, const Getter<ResourceType>& getter);
    api::metrics::Value current() const override;
    void forEach(Duration maxAge, const ValueIterator& iterator) const override;

protected:
    const ResourceType& m_resource;
    const Getter<ResourceType>& m_getter;
};

/**
 * Provides a value at any given time as well as all of the changes history.
 */
template<typename ResourceType>
class ValueHistoryMonitor: public RuntimeValueMonitor<ResourceType>
{
public:
    ValueHistoryMonitor(
        const ResourceType& resource,
        const Getter<ResourceType>& getter,
        const Watch<ResourceType>& watch);

    api::metrics::Value current() const override;
    void forEach(Duration maxAge, const ValueIterator& iterator) const override;

private:
    void updateValue();

private:
    nx::utils::ValueHistory<api::metrics::Value> m_history;
    nx::utils::SharedGuardPtr m_watchGuard;
};

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
RuntimeValueMonitor<ResourceType>::RuntimeValueMonitor(
    const ResourceType& resource,
    const Getter<ResourceType>& getter)
:
    m_resource(resource),
    m_getter(getter)
{
}

template<typename ResourceType>
api::metrics::Value RuntimeValueMonitor<ResourceType>::current() const
{
    return m_getter(m_resource);
}

template<typename ResourceType>
void RuntimeValueMonitor<ResourceType>::forEach(
    Duration maxAge, const ValueIterator& iterator) const
{
    iterator(current(), maxAge);
}

template<typename ResourceType>
ValueHistoryMonitor<ResourceType>::ValueHistoryMonitor(
    const ResourceType& resource,
    const Getter<ResourceType>& getter,
    const Watch<ResourceType>& watch)
:
    RuntimeValueMonitor<ResourceType>(resource, getter),
    m_watchGuard(watch(resource, [this](){ updateValue(); }))
{
    updateValue();
}

template<typename ResourceType>
api::metrics::Value ValueHistoryMonitor<ResourceType>::current() const
{
    return m_history.current();
}

template<typename ResourceType>
void ValueHistoryMonitor<ResourceType>::forEach(
    Duration maxAge, const ValueIterator& iterator) const
{
    m_history.forEach(maxAge, iterator);
}

template<typename ResourceType>
void ValueHistoryMonitor<ResourceType>::updateValue()
{
    const auto value = this->m_getter(this->m_resource);
    return m_history.update(value);
}

} // namespace nx::vms::utils::metrics
