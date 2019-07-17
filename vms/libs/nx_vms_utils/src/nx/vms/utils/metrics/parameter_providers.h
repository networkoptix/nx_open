#pragma once

#include "parameter_monitors.h"

namespace nx::vms::utils::metrics {

/**
 * Providers parameter manifest and allows to construct a monitor for a given resource.
 */
template<typename ResourceType>
class AbstractParameterProvider
{
public:
    AbstractParameterProvider(api::metrics::ParameterGroupManifest manifest);
    virtual ~AbstractParameterProvider() = default;

    const api::metrics::ParameterGroupManifest& manifest() const;
    virtual ParameterMonitorPtr monitor(
        const ResourceType& resource, DataBase::Writer dbWriter) const = 0;

private:
    const api::metrics::ParameterGroupManifest m_manifest;
};

/**
 * Provides single parameter.
 * If watch is provided created DataBaseParameterMonitor, otherwise RuntimeParameterMonitor only.
 */
template<typename ResourceType>
class SingleParameterProvider: public AbstractParameterProvider<ResourceType>
{
public:
    SingleParameterProvider(
        api::metrics::ParameterManifest manifest,
        Getter<ResourceType> getter,
        Watch<ResourceType> watch = nullptr);

    ParameterMonitorPtr monitor(
        const ResourceType& resource, DataBase::Writer dbWriter) const override;

private:
    const Getter<ResourceType> m_getter;
    const Watch<ResourceType> m_watch;
};

template<typename Resource>
using ResourceParameterProviders
    = std::vector<std::unique_ptr<AbstractParameterProvider<Resource>>>;

/**
 * Provides a parameter group.
 * Created a ParameterGroupMonitor with monitor for each parameter inside.
 */
template<typename ResourceType>
class ParameterGroupProvider: public AbstractParameterProvider<ResourceType>
{
public:
    ParameterGroupProvider(
        api::metrics::ParameterManifest manifest,
        ResourceParameterProviders<ResourceType> providers);

    ParameterMonitorPtr monitor(
        const ResourceType& resource, DataBase::Writer dbWriter) const override;

private:
    static api::metrics::ParameterGroupManifest makeManifest(
        api::metrics::ParameterManifest manifest,
        const ResourceParameterProviders<ResourceType>& providers);

private:
    const ResourceParameterProviders<ResourceType> m_providers;
};

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
AbstractParameterProvider<ResourceType>::AbstractParameterProvider(
    api::metrics::ParameterGroupManifest manifest)
:
    m_manifest(std::move(manifest))
{
}

template<typename ResourceType>
const api::metrics::ParameterGroupManifest&
    AbstractParameterProvider<ResourceType>::manifest() const
{
    return m_manifest;
}

template<typename ResourceType>
SingleParameterProvider<ResourceType>::SingleParameterProvider(
    api::metrics::ParameterManifest manifest,
    Getter<ResourceType> getter,
    Watch<ResourceType> watch)
:
    AbstractParameterProvider<ResourceType>(api::metrics::makeParameterManifest(
        std::move(manifest.id), std::move(manifest.name), std::move(manifest.unit))),
    m_getter(std::move(getter)),
    m_watch(std::move(watch))
{
}

template<typename ResourceType>
ParameterMonitorPtr SingleParameterProvider<ResourceType>::monitor(
    const ResourceType& resource, DataBase::Writer dbWriter) const
{
    if (m_watch)
    {
        return std::make_unique<DataBaseParameterMonitor<ResourceType>>(
            resource, m_getter, dbWriter, m_watch);
    }
    else
    {
        return std::make_unique<RuntimeParameterMonitor<ResourceType>>(
            resource, m_getter);
    }
}

template<typename ResourceType>
ParameterGroupProvider<ResourceType>::ParameterGroupProvider(
    api::metrics::ParameterManifest manifest,
    ResourceParameterProviders<ResourceType> providers)
:
    AbstractParameterProvider<ResourceType>(makeManifest(std::move(manifest), providers)),
    m_providers(std::move(providers))
{
}

template<typename ResourceType>
ParameterMonitorPtr ParameterGroupProvider<ResourceType>::monitor(
    const ResourceType& resource, DataBase::Writer dbWriter) const
{
    std::map<QString, ParameterMonitorPtr> monitors;
    for (const auto& provider: m_providers)
    {
        const auto id = provider->manifest().id;
        monitors[id] = provider->monitor(resource, dbWriter[id]);
    }

    return std::make_unique<ParameterGroupMonitor<ResourceType>>(std::move(monitors));
}

template<typename Resource>
api::metrics::ParameterGroupManifest ParameterGroupProvider<Resource>::makeManifest(
    api::metrics::ParameterManifest manifest,
    const ResourceParameterProviders<Resource>& providers)
{
    auto groupManifest = api::metrics::makeParameterGroupManifest(
        std::move(manifest.id), std::move(manifest.name));

    for (const auto& provider: providers)
        groupManifest.group.push_back(provider->manifest());

    return groupManifest;
}

} // namespace nx::vms::server::metrics
