#pragma once

#include <nx/utils/log/log.h>

#include "parameter_providers.h"

namespace nx::vms::utils::metrics {

struct ResourceDescription
{
    QString id;
    QString parent;
    QString name;
    bool isLocal = true;

    ResourceDescription(QString id, QString parent, QString name, bool isLocal = true):
        id(std::move(id)), parent(std::move(parent)), name(std::move(name)), isLocal(isLocal) {}
};

class AbstractResourceProvider
{
public:
    virtual ~AbstractResourceProvider() = default;

    /** Returns manifest for this kind of resource. */
    virtual const api::metrics::ResourceManifest& manifest() const = 0;

    /** Starts monitoring for new resources. dataBaseAccess should be used to store their values. */
    virtual void startMonitoring(DataBase::Writer dbWriter) = 0;

    /**
     * Returns current resources with parameter values.
     * @param includeRemote If true the list of all known resourses is returned.
     * @param length If specified values are returned in a form dictionary with timestamps.
     */
    virtual api::metrics::ResourceGroupValues values(
        bool includeRemote, std::optional<std::chrono::milliseconds> timeline) const = 0;
};

/**
 * AbstractResourceProvider implementation helper.
 */
template<typename ResourceType>
class ResourceProvider: public AbstractResourceProvider
{
public:
    using Resource = ResourceType;
    ResourceProvider(ResourceParameterProviders<Resource> providers);

    using Value = api::metrics::Value;
    using ParameterProviders = ResourceParameterProviders<Resource>;
    using ParameterProviderPtr = std::unique_ptr<AbstractParameterProvider<Resource>>;

    const api::metrics::ResourceManifest& manifest() const final;
    void startMonitoring(DataBase::Writer dbWriter) final;

    api::metrics::ResourceGroupValues values(
        bool includeRemote, std::optional<std::chrono::milliseconds> timeline) const final;

protected:
    /** Should be implemented in inheritors to begin resource discovery process. */
    virtual void startMonitoring() = 0;

    /** Should be implemented in inheritors to provide resource description. */
    virtual std::optional<ResourceDescription> describe(const Resource& resource) const = 0;

    /** Should be called from inheritors, when new resource is found. */
    void found(const Resource& resource);

    /** Should be called from inheritors, when resource is lost. */
    void lost(const Resource& resource);

    /** Should be called from inheritors, when resource description is changed. */
    void changed(const Resource& resource);

    static std::unique_ptr<AbstractParameterProvider<ResourceType>> singleParameterProvider(
        api::metrics::ParameterManifest manifest,
        Getter<ResourceType> getter, Watch<ResourceType> watch = nullptr);

    template<typename... Providers>
    static ResourceParameterProviders<Resource> parameterProviders(Providers... providers);

    template<typename... Providers>
    static std::unique_ptr<AbstractParameterProvider<Resource>> parameterGroupProvider(
        api::metrics::ParameterManifest manifest, Providers... providers);

private:
    const ParameterGroupProvider<Resource> m_parameters;
    mutable nx::utils::Mutex m_mutex;
    DataBase::Writer m_dbWriter;
    std::map<Resource, ParameterMonitorPtr> m_resources;
};

// -----------------------------------------------------------------------------------------------

template<typename ResourceType>
ResourceProvider<ResourceType>::ResourceProvider(ResourceParameterProviders<Resource> providers):
    m_parameters({}, std::move(providers))
{
}

template<typename ResourceType>
const api::metrics::ResourceManifest& ResourceProvider<ResourceType>::manifest() const
{
    return m_parameters.manifest().group;
}

template<typename ResourceType>
void ResourceProvider<ResourceType>::startMonitoring(DataBase::Writer dbWriter)
{
    m_dbWriter = dbWriter;
    startMonitoring();
}

template<typename ResourceType>
api::metrics::ResourceGroupValues ResourceProvider<ResourceType>::values(
    bool includeRemote, std::optional<std::chrono::milliseconds> timeline) const
{
    api::metrics::ResourceGroupValues groupValues;
    for (const auto& [resource, monitor]: m_resources)
    {
        if (auto description = describe(resource))
        {
            if (!includeRemote && !description->isLocal)
                continue;

            api::metrics::ParameterGroupValues parameterValues;
            if (timeline)
            {
                auto timelineValues = monitor->timeline(*timeline);
                if (!timelineValues)
                    continue;

                parameterValues = std::move(*timelineValues);
            }
            else
            {
                parameterValues = monitor->current();
            }

            if (!NX_ASSERT(!parameterValues.group.empty()))
                continue;

            groupValues[description->id] = {
                std::move(description->name),
                std::move(description->parent),
                std::move(parameterValues.group)
            };
        }
    }

    return groupValues;
}

template<typename ResourceType>
void ResourceProvider<ResourceType>::found(const Resource& resource)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        const auto [it, isInserted] =  m_resources.emplace(resource, ParameterMonitorPtr{});
        if (!NX_ASSERT(isInserted, "Duplicate %1", resource))
            return;
    }

    NX_VERBOSE(this, "Found %1", resource);
    changed(resource);
}

template<typename ResourceType>
void ResourceProvider<ResourceType>::lost(const Resource& resource)
{

    NX_VERBOSE(this, "Lost %1", resource);
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_resources.erase(resource);
}

template<typename ResourceType>
void ResourceProvider<ResourceType>::changed(const Resource& resource)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto& monitor = m_resources[resource];
    if (const auto description = describe(resource))
    {
        if (!monitor)
        {
            NX_DEBUG(this, "Start monitoring %1", resource);
            monitor = m_parameters.monitor(resource, m_dbWriter[description->id]);
        }
    }
    else
    {
        if (monitor)
        {
            NX_DEBUG(this, "Stop monitoring %1", resource);
            monitor.reset();
        }
    }
}

template<typename ResourceType>
std::unique_ptr<AbstractParameterProvider<ResourceType>>
    ResourceProvider<ResourceType>::singleParameterProvider(
        api::metrics::ParameterManifest manifest,
        Getter<ResourceType> getter, Watch<ResourceType> watch)
{
    return std::make_unique<SingleParameterProvider<Resource>>(
        std::move(manifest), std::move(getter), std::move(watch));
}

template<typename ResourceType>
template<typename... Providers>
ResourceParameterProviders<ResourceType>
    ResourceProvider<ResourceType>::parameterProviders(Providers... providers)
{
    ResourceParameterProviders<Resource> proviers;
    (proviers.push_back(std::forward<Providers>(providers)), ...);
    return proviers;
}

template<typename ResourceType>
template<typename... Providers>
std::unique_ptr<AbstractParameterProvider<ResourceType>>
    ResourceProvider<ResourceType>::parameterGroupProvider(
        api::metrics::ParameterManifest manifest, Providers... providers)
{
    return std::make_unique<ParameterGroupProvider<Resource>>(
        std::move(manifest), parameterProviders(std::forward<Providers>(providers)...));
}

} // namespace nx::vms::utils::metrics
