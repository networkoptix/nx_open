// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/std/cppnx.h>

#include "value_group_monitor.h"
#include "value_monitors.h"

namespace nx::vms::utils::metrics {

/**
 * Provides single parameter.
 * If watch is provided created DataBaseParameterMonitor, otherwise RuntimeParameterMonitor only.
 */
template<typename ResourceType>
class ValueProvider
{
public:
    ValueProvider(
        Scope scope,
        QString id,
        Getter<ResourceType> getter,
        Watch<ResourceType> watch = nullptr);

    const QString& id() const { return m_id; }
    std::unique_ptr<ValueMonitor> monitor(const ResourceType& resource, Scope resourceScope) const;

private:
    const Scope m_scope = Scope::local;
    const QString m_id;
    const Getter<ResourceType> m_getter;
    const Watch<ResourceType> m_watch;
};

template<typename ResourceType>
using ValueProviders = std::vector<std::unique_ptr<ValueProvider<ResourceType>>>;

template<typename ResourceType, typename... Args>
std::unique_ptr<ValueProvider<ResourceType>> makeLocalValueProvider(Args... args);

template<typename ResourceType, typename... Args>
std::unique_ptr<ValueProvider<ResourceType>> makeSystemValueProvider(Args... args);

/**
 * Provides a parameter group.
 * Creates a ParameterGroupMonitor with monitor for each parameter inside.
 */
template<typename ResourceType>
class ValueGroupProvider
{
public:
    ValueGroupProvider(QString id, ValueProviders<ResourceType> providers);

    template<typename... Providers>
    ValueGroupProvider(QString id, Providers... providers);

    const QString& id() const { return m_id; }
    std::vector<QString> children() const;
    std::unique_ptr<ValueGroupMonitor> monitor(const ResourceType& resource, Scope resourceScope) const;

private:
    const QString m_id;
    ValueProviders<ResourceType> m_providers;
};

template<typename ResourceType>
using ValueGroupProviders = std::vector<std::unique_ptr<ValueGroupProvider<ResourceType>>>;

template<typename ResourceType, typename... Args>
std::unique_ptr<ValueGroupProvider<ResourceType>> makeValueGroupProvider(Args... args);

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
ValueProvider<ResourceType>::ValueProvider(
    Scope scope,
    QString id,
    Getter<ResourceType> getter,
    Watch<ResourceType> watch)
:
    m_scope(scope),
    m_id(std::move(id)),
    m_getter(std::move(getter)),
    m_watch(std::move(watch))
{
}

template<typename ResourceType>
std::unique_ptr<ValueMonitor> ValueProvider<ResourceType>::monitor(
    const ResourceType& resource, Scope resourceScope) const
{
    if (resourceScope == Scope::site && m_scope == Scope::local)
        return nullptr;

    if (!m_watch)
    {
        return std::make_unique<RuntimeValueMonitor<ResourceType>>(
            m_id, m_scope, resource, m_getter);
    }

    return std::make_unique<ValueHistoryMonitor<ResourceType>>(
        m_id, m_scope, resource, m_getter, m_watch);
}

template<typename ResourceType, typename... Args>
std::unique_ptr<ValueProvider<ResourceType>> makeLocalValueProvider(Args... args)
{
    return std::make_unique<ValueProvider<ResourceType>>(Scope::local, std::forward<Args>(args)...);
}

template<typename ResourceType, typename... Args>
std::unique_ptr<ValueProvider<ResourceType>> makeSystemValueProvider(Args... args)
{
    return std::make_unique<ValueProvider<ResourceType>>(Scope::site, std::forward<Args>(args)...);
}

template<typename ResourceType>
ValueGroupProvider<ResourceType>::ValueGroupProvider(
    QString id,
    std::vector<std::unique_ptr<ValueProvider<ResourceType>>> providers)
:
    m_id(id),
    m_providers(std::move(providers))
{
}

template<typename ResourceType>
template<typename... Providers>
ValueGroupProvider<ResourceType>::ValueGroupProvider(QString id, Providers... providers):
    ValueGroupProvider(
        id, nx::utils::make_container<ValueProviders<ResourceType>>(std::move(providers)...))
{
}

template<typename ResourceType>
std::vector<QString> ValueGroupProvider<ResourceType>::children() const
{
    std::vector<QString> ids;
    for (const auto& provider: m_providers)
        ids.push_back(provider->id());
    return ids;
}

template<typename ResourceType>
std::unique_ptr<ValueGroupMonitor> ValueGroupProvider<ResourceType>::monitor(
    const ResourceType& resource, Scope scope) const
{
    std::map<QString, std::unique_ptr<ValueMonitor>> monitors;
    for (const auto& provider: m_providers)
    {
        if (auto monitor = provider->monitor(resource, scope))
            monitors.emplace(provider->id(), std::move(monitor));
    }

    if (monitors.empty())
        return nullptr;

    return std::make_unique<ValueGroupMonitor>(std::move(monitors));
}

template<typename ResourceType, typename... Args>
std::unique_ptr<ValueGroupProvider<ResourceType>> makeValueGroupProvider(Args... args)
{
    return std::make_unique<ValueGroupProvider<ResourceType>>(std::forward<Args>(args)...);
}

} // namespace nx::vms::server::metrics
