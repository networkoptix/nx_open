// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/metrics.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/value_history.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/exception.h>
#include <nx/utils/log/log.h>

#include "resource_description.h"

namespace nx::vms::utils::metrics {

// NOTE: Inherited from std::runtime_error to have a constructor from a string.
class BaseError: public std::runtime_error { using std::runtime_error::runtime_error; };
class ExpectedError: public BaseError { using BaseError::BaseError; };

#define NX_METRICS_EXPECTED_ERROR(EXPRESSION, EXCEPTION, MESSAGE) \
    NX_WRAP_EXCEPTION(EXPRESSION, EXCEPTION, nx::vms::utils::metrics::ExpectedError, MESSAGE)

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
    virtual void setOptional(bool isOptional);

    Scope scope() const;
    void setScope(Scope scope);

    api::metrics::Value value() const noexcept;

    virtual void forEach(Duration maxAge, const ValueIterator& iterator, Border border) const = 0;

    void setFormatter(ValueFormatter formatter);
    api::metrics::Value formattedValue() const noexcept;

    QString idForToStringFromPtr() const;

protected:
    virtual api::metrics::Value valueOrThrow() const noexcept(false) = 0;
    api::metrics::Value handleValueErrors(std::function<api::metrics::Value()> calculateValue) const;

private:
    const QString m_name; // TODO: better to have full name?
    std::atomic<Scope> m_scope = Scope::local;
    std::atomic<bool> m_optional = false;
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

private:
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

    virtual void setOptional(bool isOptional) override;
    virtual api::metrics::Value valueOrThrow() const override;

private:
    void updateValue();

private:
    nx::utils::ValueHistory<api::metrics::Value> m_history;
    nx::utils::SharedGuardPtr m_watchGuard;

    // NOTE: This variable is used to assert on suspicious situations in `setOptional` method when
    // this monitor is initially set to optional and then is switched to not optional.
    bool m_setOptionalFirstTime = true;
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
    m_history(std::chrono::hours(24)), //< The longest period supported by current metrics system.
    m_watchGuard(watch(resource, [this](){ updateValue(); }))
{
    // NOTE: Required to override default value, because updateValue() is called here before real
    // optionality mark was set.
    ValueMonitor::setOptional(true);
    updateValue();
}

template<typename ResourceType>
void ValueHistoryMonitor<ResourceType>::forEach(
    Duration maxAge, const ValueIterator& iterator, Border border) const
{
    m_history.forEach(maxAge, iterator, border);
}

template<typename ResourceType>
api::metrics::Value ValueHistoryMonitor<ResourceType>::valueOrThrow() const
{
    return m_history.current();
}

template<typename ResourceType>
void ValueHistoryMonitor<ResourceType>::setOptional(bool isOptional)
{
    // NOTE: Next code is written to assert on suspicious situations when this monitor is initially
    // set to optional and then is switched to not optional.
    if (m_setOptionalFirstTime)
    {
        m_setOptionalFirstTime = false;
    }
    else
    {
        NX_ASSERT(!this->optional() || isOptional,
            "%1 is switched to not optional, probably by mistake",
            this);
    }
    ValueMonitor::setOptional(isOptional);
}

template<typename ResourceType>
void ValueHistoryMonitor<ResourceType>::updateValue()
{
    // NOTE: Should call RuntimeValueMonitor::valueOrThrow() because
    // ValueHistoryMonitor::valueOrThrow() is reimplemented to return value from the history.
    m_history.update(this->handleValueErrors(
        [this](){ return RuntimeValueMonitor<ResourceType>::valueOrThrow(); }));
}

} // namespace nx::vms::utils::metrics
