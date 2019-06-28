#pragma once

#include "parameter_providers.h"

namespace nx::vms::utils::metrics {

struct ResourceDescription
{
    QString id;
    QString parent;
    QString name;
};

class AbstractResourceProvider
{
public:
    virtual ~AbstractResourceProvider() = default;

    /** Returns manifest for this kind of resource. */
    virtual const api::metrics::ResourceManifest& manifest() const = 0;

    /** Starts monitoring for new resources. dataBaseAccess should be used to store their values. */
    virtual void startMonitoring(DataBase::Access dataBaseAccess) = 0;

    /** Resource descriptions for resources, which are currently discovered. */
    virtual std::vector<ResourceDescription> resources() const = 0;
};

/**
 * AbstractResourceProvider implementation helper.
 */
template<typename Resource>
class ResourceProvider: public AbstractResourceProvider
{
public:
    ResourceProvider(ResourceParameterProviders<Resource> providers);

    const api::metrics::ResourceManifest& manifest() const final;
    void startMonitoring(DataBase::Access dataBaseAccess) final;
    std::vector<ResourceDescription> resources() const final;

    using Value = api::metrics::Value;
    using ParameterProviders = ResourceParameterProviders<Resource>;
    using ParameterProviderPtr = std::unique_ptr<AbstractParameterProvider<Resource>>;

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

    static std::unique_ptr<AbstractParameterProvider<Resource>> singleParameterProvider(
        api::metrics::ParameterManifest manifest,
        typename SingleParameterProvider<Resource>::Getter getter,
        typename SingleParameterProvider<Resource>::Watch watch = nullptr);

    template<typename... Providers>
    static ResourceParameterProviders<Resource> parameterProviders(Providers... providers);

    template<typename... Providers>
    static std::unique_ptr<AbstractParameterProvider<Resource>> parameterGroupProvider(
        api::metrics::ParameterManifest manifest, Providers... providers);

private:
    const ParameterGroupProvider<Resource> m_parameters;
    mutable nx::utils::Mutex m_mutex;
    DataBase::Access m_dataBaseAccess;
    std::map<Resource, nx::utils::SharedGuardPtr> m_resources;
};

// -----------------------------------------------------------------------------------------------

template<typename Resource>
ResourceProvider<Resource>::ResourceProvider(ResourceParameterProviders<Resource> providers):
    m_parameters({}, std::move(providers))
{
}

template<typename Resource>
const api::metrics::ResourceManifest& ResourceProvider<Resource>::manifest() const
{
    return m_parameters.manifest().group;
}

template<typename Resource>
void ResourceProvider<Resource>::startMonitoring(DataBase::Access dataBaseAccess)
{
    m_dataBaseAccess = dataBaseAccess;
    startMonitoring();
}

template<typename Resource>
std::vector<ResourceDescription> ResourceProvider<Resource>::resources() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    std::vector<ResourceDescription> descriptions;
    for (const auto [resource, monitorGuard]: m_resources)
    {
        if (auto description = describe(resource))
            descriptions.push_back(std::move(*description));
    }

    return descriptions;
}

template<typename Resource>
void ResourceProvider<Resource>::found(const Resource& resource)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        const auto [it, isInserted] =  m_resources.emplace(resource, nx::utils::SharedGuardPtr());
        if (!NX_ASSERT(isInserted, "Duplicate %1", resource))
            return;
    }

    NX_VERBOSE(this, "Found %1", resource);
    changed(resource);
}

template<typename Resource>
void ResourceProvider<Resource>::lost(const Resource& resource)
{

    NX_VERBOSE(this, "Lost %1", resource);
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_resources.erase(resource);
}

template<typename Resource>
void ResourceProvider<Resource>::changed(const Resource& resource)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    nx::utils::SharedGuardPtr& monitoringGuard = m_resources[resource];
    if (const auto description = describe(resource))
    {
        const auto& id = description->id;
        if (!monitoringGuard)
        {
            NX_DEBUG(this, "Start monitoring %1", resource);
            auto guard = m_parameters.monitor(resource, m_dataBaseAccess[id]);
            monitoringGuard = guard;
        }
    }
    else
    {
        if (monitoringGuard)
        {
            NX_DEBUG(this, "Stop monitoring %1", resource);
            monitoringGuard.reset();
        }
    }
}

template<typename Resource>
std::unique_ptr<AbstractParameterProvider<Resource>>
    ResourceProvider<Resource>::singleParameterProvider(
        api::metrics::ParameterManifest manifest,
        typename SingleParameterProvider<Resource>::Getter getter,
        typename SingleParameterProvider<Resource>::Watch watch)
{
    return std::make_unique<SingleParameterProvider<Resource>>(
        std::move(manifest), std::move(getter), std::move(watch));
}

template<typename Resource>
template<typename... Providers>
ResourceParameterProviders<Resource>
    ResourceProvider<Resource>::parameterProviders(Providers... providers)
{
    ResourceParameterProviders<Resource> proviers;
    (proviers.push_back(std::forward<Providers>(providers)), ...);
    return proviers;
}

template<typename Resource>
template<typename... Providers>
std::unique_ptr<AbstractParameterProvider<Resource>>
    ResourceProvider<Resource>::parameterGroupProvider(
        api::metrics::ParameterManifest manifest, Providers... providers)
{
    return std::make_unique<ParameterGroupProvider<Resource>>(
        std::move(manifest), parameterProviders(std::forward<Providers>(providers)...));
}

} // namespace nx::vms::utils::metrics
