#pragma once

#include "resource_monitor.h"
#include "value_providers.h"

namespace nx::vms::utils::metrics {

/**
 * Provides a parameter group.
 * Creates a ResourceMonitor with group monitors inside.
 */
template<typename ResourceType>
class ResourceProvider
{
public:
    ResourceProvider(ValueGroupProviders<ResourceType> providers);

    api::metrics::ResourceManifest manifest() const;
    std::unique_ptr<ResourceMonitor> monitor(
        std::unique_ptr<TypedResourceDescription<ResourceType>> resource) const;

private:
    ValueGroupProviders<ResourceType> m_providers;
};

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
ResourceProvider<ResourceType>::ResourceProvider(ValueGroupProviders<ResourceType> providers):
    m_providers(std::move(providers))
{
}

template<typename ResourceType>
api::metrics::ResourceManifest ResourceProvider<ResourceType>::manifest() const
{
    api::metrics::ResourceManifest groups;
    for (const auto& provider: m_providers)
    {
        api::metrics::ValueGroupManifest group(api::metrics::Label{provider->id()});
        for (const auto& id: provider->children())
            group.values.push_back(api::metrics::Label{id});
        groups.push_back(std::move(group));
    }
    return groups;
}

template<typename ResourceType>
std::unique_ptr<ResourceMonitor> ResourceProvider<ResourceType>::monitor(
    std::unique_ptr<TypedResourceDescription<ResourceType>> description) const
{
    ValueGroupMonitors monitors;
    for (const auto& provider: m_providers)
    {
        if (auto monitor = provider->monitor(description->resource, description->scope))
            monitors.emplace(provider->id(), std::move(monitor));
    }

    NX_ASSERT(monitors.size());
    return std::make_unique<ResourceMonitor>(std::move(description), std::move(monitors));
}

} // namespace nx::vms::utils::metrics
