#pragma once

#include <nx/utils/std/cppnx.h>

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
        api::metrics::ValueManifest manifest,
        Getter<ResourceType> getter,
        Watch<ResourceType> watch = nullptr);

    const QString& id() const { return m_manifest.id; }

    api::metrics::ValueManifest manifest() const { return m_manifest; }
    std::unique_ptr<ValueMonitor> monitor(const ResourceType& resource) const;

private:
    const api::metrics::ValueManifest m_manifest;
    const Getter<ResourceType> m_getter;
    const Watch<ResourceType> m_watch;
};

template<typename ResourceType>
using ValueProviders = std::vector<std::unique_ptr<ValueProvider<ResourceType>>>;

/**
 * Provides a parameter group.
 * Creates a ParameterGroupMonitor with monitor for each parameter inside.
 */
template<typename ResourceType>
class ValueGroupProvider
{
public:
    ValueGroupProvider(api::metrics::Label label, ValueProviders<ResourceType> providers);

    template<typename... Providers>
    ValueGroupProvider(api::metrics::Label label, Providers... providers):
        ValueGroupProvider(
            std::move(label),
            nx::utils::make_container<ValueProviders<ResourceType>>(std::move(providers)...))
    {
    }

    const QString& id() const { return m_label.id; }

    api::metrics::ValueGroupManifest manifest() const;
    std::unique_ptr<ValueGroupMonitor> monitor(const ResourceType& resource) const;

private:
    const api::metrics::Label m_label;
    ValueProviders<ResourceType> m_providers;
};

template<typename ResourceType>
using ValueGroupProviders = std::vector<std::unique_ptr<ValueGroupProvider<ResourceType>>>;

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
ValueProvider<ResourceType>::ValueProvider(
    api::metrics::ValueManifest manifest,
    Getter<ResourceType> getter,
    Watch<ResourceType> watch)
:
    m_manifest(std::move(manifest)),
    m_getter(std::move(getter)),
    m_watch(std::move(watch))
{
}

template<typename ResourceType>
std::unique_ptr<ValueMonitor> ValueProvider<ResourceType>::monitor(const ResourceType& resource) const
{
    if (m_watch)
        return std::make_unique<ValueHistoryMonitor<ResourceType>>(resource, m_getter, m_watch);
    else
        return std::make_unique<RuntimeValueMonitor<ResourceType>>(resource, m_getter);
}

template<typename ResourceType>
ValueGroupProvider<ResourceType>::ValueGroupProvider(
    api::metrics::Label label,
    std::vector<std::unique_ptr<ValueProvider<ResourceType>>> providers)
:
    m_label(std::move(label)),
    m_providers(std::move(providers))
{
}

template<typename ResourceType>
api::metrics::ValueGroupManifest ValueGroupProvider<ResourceType>::manifest() const
{
    api::metrics::ValueGroupManifest manifest(m_label);
    for (const auto& provider: m_providers)
        manifest.values.push_back(provider->manifest());

    return manifest;
}

template<typename ResourceType>
std::unique_ptr<ValueGroupMonitor> ValueGroupProvider<ResourceType>::monitor(
    const ResourceType& resource) const
{
    std::map<QString, std::unique_ptr<ValueMonitor>> monitors;
    for (const auto& provider: m_providers)
    {
        const auto id = provider->manifest().id;
        monitors[id] = provider->monitor(resource);
    }

    return std::make_unique<ValueGroupMonitor>(std::move(monitors));
}

} // namespace nx::vms::server::metrics
