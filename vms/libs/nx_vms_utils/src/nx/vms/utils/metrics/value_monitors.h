#pragma once

#include <nx/vms/api/metrics.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/value_history.h>
#include <nx/utils/move_only_func.h>

#include "resource_description.h"

namespace nx::vms::utils::metrics {

using Border = nx::utils::ValueHistory<api::metrics::Value>::Border;
using Duration = std::chrono::milliseconds;
using ValueIterator = std::function<void(const api::metrics::Value& value, Duration age)>;
using ValueFormatter = std::function<api::metrics::Value(const api::metrics::Value& value)>;

/**
 * Allows to monitor parameter value.
 */
class NX_VMS_UTILS_API ValueMonitor
{
public:
    explicit ValueMonitor(Scope scope, QString name): m_scope(scope), m_name(std::move(name)) {}
    virtual ~ValueMonitor() = default;

    QString name() const { return m_name; }
    Scope scope() const { return m_scope; }
    void setScope(Scope scope) { m_scope = scope; }

    virtual api::metrics::Value value() const noexcept(false) = 0;
    virtual void forEach(Duration maxAge, const ValueIterator& iterator, Border border) const = 0;

    void setFormatter(ValueFormatter formatter) { m_formatter = std::move(formatter); };
    api::metrics::Value formattedValue() const noexcept(false)
    {
        return m_formatter ? m_formatter(value()) : value();
    }

    QString toString() const { return lm("%1(%2)").args(m_name, m_scope); }

private:
    Scope m_scope = Scope::local;
    QString m_name; // TODO: better to have full name?
    ValueFormatter m_formatter;
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
    RuntimeValueMonitor(
        Scope scope,
        QString name,
        const ResourceType& resource,
        const Getter<ResourceType>& getter);

    api::metrics::Value value() const override;
    void forEach(Duration maxAge, const ValueIterator& iterator, Border border) const override;

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
        Scope scope,
        QString name,
        const ResourceType& resource,
        const Getter<ResourceType>& getter,
        const Watch<ResourceType>& watch);

    api::metrics::Value value() const override;
    void forEach(Duration maxAge, const ValueIterator& iterator, Border border) const override;

private:
    void updateValue();

private:
    nx::utils::ValueHistory<api::metrics::Value> m_history;
    nx::utils::SharedGuardPtr m_watchGuard;
};

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
RuntimeValueMonitor<ResourceType>::RuntimeValueMonitor(
    Scope scope,
    QString name,
    const ResourceType& resource,
    const Getter<ResourceType>& getter)
:
    ValueMonitor(scope, std::move(name)),
    m_resource(resource),
    m_getter(getter)
{
}

template<typename ResourceType>
api::metrics::Value RuntimeValueMonitor<ResourceType>::value() const
{
    return m_getter(m_resource);
}

template<typename ResourceType>
void RuntimeValueMonitor<ResourceType>::forEach(
    Duration maxAge, const ValueIterator& iterator, Border /*border*/) const
{
    iterator(value(), maxAge);
}

template<typename ResourceType>
ValueHistoryMonitor<ResourceType>::ValueHistoryMonitor(
    Scope scope,
    QString name,
    const ResourceType& resource,
    const Getter<ResourceType>& getter,
    const Watch<ResourceType>& watch)
:
    RuntimeValueMonitor<ResourceType>(scope, std::move(name), resource, getter),
    m_watchGuard(watch(resource, [this](){ updateValue(); }))
{
    updateValue();
}

template<typename ResourceType>
api::metrics::Value ValueHistoryMonitor<ResourceType>::value() const
{
    return m_history.current();
}

template<typename ResourceType>
void ValueHistoryMonitor<ResourceType>::forEach(
    Duration maxAge, const ValueIterator& iterator, Border border) const
{
    m_history.forEach(maxAge, iterator, border);
}

template<typename ResourceType>
void ValueHistoryMonitor<ResourceType>::updateValue()
{
    const auto value = this->m_getter(this->m_resource);
    return m_history.update(value);
}

} // namespace nx::vms::utils::metrics
