#pragma once

#include <nx/vms/api/metrics.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/value_history.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/exceptions.h>
#include <nx/utils/log/log.h>

#include "resource_description.h"

namespace nx::vms::utils::metrics {

// NOTE: Inherited from std::runtime_error to have a constructor from a string.
class MetricsError: public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class ExpectedError: public MetricsError
{
    using MetricsError::MetricsError;
};

#define EXPECTED_ERROR(EXPRESSION, EXCEPTION, MESSAGE) \
    WRAP_EXCEPTION(EXPRESSION, EXCEPTION, nx::vms::utils::metrics::ExpectedError, MESSAGE)

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
    explicit ValueMonitor(QString name, Scope scope);
    virtual ~ValueMonitor() = default;

    QString name() const;

    bool optional() const;
    void setOptional(bool isOptional);

    Scope scope() const;
    void setScope(Scope scope);

    api::metrics::Value value() const noexcept;

    virtual void forEach(Duration maxAge, const ValueIterator& iterator, Border border) const = 0;

    void setFormatter(ValueFormatter formatter);
    api::metrics::Value formattedValue() const noexcept;

    QString toString() const;
    QString idForToStringFromPtr() const;

protected:
    virtual api::metrics::Value valueOrThrow() const noexcept(false) = 0;

private:
    QString m_name; // TODO: better to have full name?
    Scope m_scope = Scope::local;
    bool m_optional = false;
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
        QString name,
        Scope scope,
        const ResourceType& resource,
        const Getter<ResourceType>& getter);

    void forEach(Duration maxAge, const ValueIterator& iterator, Border border) const override;

protected:
    virtual api::metrics::Value valueOrThrow() const override;

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
        QString name,
        Scope scope,
        const ResourceType& resource,
        const Getter<ResourceType>& getter,
        const Watch<ResourceType>& watch);

    void forEach(Duration maxAge, const ValueIterator& iterator, Border border) const override;

protected:
    virtual api::metrics::Value valueOrThrow() const override;

private:
    void updateValue();

private:
    nx::utils::ValueHistory<api::metrics::Value> m_history;
    nx::utils::SharedGuardPtr m_watchGuard;
};

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
RuntimeValueMonitor<ResourceType>::RuntimeValueMonitor(
    QString name,
    Scope scope,
    const ResourceType& resource,
    const Getter<ResourceType>& getter)
:
    ValueMonitor(std::move(name), scope),
    m_resource(resource),
    m_getter(getter)
{
}

template<typename ResourceType>
api::metrics::Value RuntimeValueMonitor<ResourceType>::valueOrThrow() const
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
    QString name,
    Scope scope,
    const ResourceType& resource,
    const Getter<ResourceType>& getter,
    const Watch<ResourceType>& watch)
:
    RuntimeValueMonitor<ResourceType>(std::move(name), scope, resource, getter),
    m_watchGuard(watch(resource, [this](){ updateValue(); }))
{
    updateValue();
}

template<typename ResourceType>
api::metrics::Value ValueHistoryMonitor<ResourceType>::valueOrThrow() const
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
