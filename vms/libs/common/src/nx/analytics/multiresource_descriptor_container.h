#pragma once

#include <nx/analytics/descriptor_container.h>
#include <nx/analytics/property_descriptor_storage.h>
#include <utils/common/value_cache.h>

namespace nx::analytics {

template<typename StorageFactoryType, typename MergeExecutorType>
class MultiresourceDescriptorContainer
{
public:
    using StorageFactory = StorageFactoryType;
    using MergeExecutor = MergeExecutorType;
    using Container = DescriptorContainer<
        typename decltype(
            std::declval<StorageFactory>()(
                QnResourcePtr(),
                std::function<void()>()))::element_type,
        MergeExecutor>;

    using DescriptorMap = typename decltype(std::declval<Container>().descriptors())::value_type;
    using TopLevelMap = typename MapHelper::Wrapper<DescriptorMap>::template wrap<QnUuid>;

public:
    MultiresourceDescriptorContainer(
        StorageFactory storageFactory,
        const QnResourceList& resourceList,
        QnResourcePtr ownResource)
        :
        m_cachedMergedDescriptors(
            [this]() { return mergedDescriptorsInternal(); },
            &m_mutex)
    {
        const auto notifyWhenUpdated = [this]() { m_cachedMergedDescriptors.reset(); };
        for (auto& resource: resourceList)
        {
            auto storage = storageFactory(resource, notifyWhenUpdated);
            m_containers[resource->getId()] = std::make_unique<Container>(std::move(storage));
        }

        if (!ownResource)
            return;

        auto writableStorage = storageFactory(ownResource, notifyWhenUpdated);
        m_ownResourceId = ownResource->getId();
        m_containers[m_ownResourceId] =
            std::make_unique<Container>(std::move(writableStorage));
    }

    /**
     * Returns descriptors from all container resources merged into a single map with the help of
     * merge executor.
     */
    std::optional<TopLevelMap> descriptors() const
    {
        TopLevelMap result;
        for (const auto&[resourceId, container]: m_containers)
        {
            auto descriptors = container->descriptors();
            if (descriptors)
                result[resourceId] = std::move(*descriptors);
        }

        if (result.empty())
            return std::nullopt;

        return result;
    }

    /**
     * Returns descriptors grouped by a resource.
     */
    template<typename... Scopes>
    auto descriptors(const QnUuid& resourceId, const Scopes&... scopes) const
        -> std::optional<MapHelper::MappedTypeOnLevel<DescriptorMap, sizeof...(Scopes)>>
    {
        auto itr = m_containers.find(resourceId);
        if (itr == m_containers.cend())
            return std::nullopt;

        auto& container = itr->second;
        return container->descriptors(scopes...);
    }

    /**
     * Returns descriptors from all container resources merged into a single map with the help of
     * merge executor.
     */
    template<typename... Scopes>
    auto mergedDescriptors(const Scopes&... scopes) const
        ->std::optional<MapHelper::MappedTypeOnLevel<DescriptorMap, sizeof...(Scopes)>>
    {
        return MapHelper::getOptional(m_cachedMergedDescriptors.get(), scopes...);
    }

    /**
     * Removes descriptors by the specified scope chain. Affects only the own resource (usually it
     * is the current server) descriptor map. Method is prohibited to be called from the client
     * side.
     */
    template<typename... Scopes>
    void removeDescriptors(const Scopes&... scopes)
    {
        auto container = ownResourceContainer();
        if (!NX_ASSERT(container))
            return;

        container->removeDescriptors(scopes...);
    }

    /**
     * Inserts or replaces descriptors by the specified scope chain. Affects only the own resource
     * (usually it is the current server) descriptor map. Method is prohibited to be called from
     * the client side.
     */
    template<typename Descriptors, typename... Scopes>
    void setDescriptors(Descriptors&& descriptors, const Scopes&... scopes)
    {
        auto container = ownResourceContainer();
        if (!NX_ASSERT(container))
            return;

        container->setDescriptors(std::forward<Descriptors>(descriptors), scopes...);
    }

    /**
     * Merges provided descriptors with the current ones with the help of merge executor.
     * Affects only the own resource (usually it is the current server) descriptor map. Method is
     * prohibited to be called from the client side.
     */
    template<typename Descriptors, typename... Scopes>
    void mergeWithDescriptors(Descriptors&& descriptors, const Scopes&... scopes)
    {
        auto container = ownResourceContainer();
        if (!NX_ASSERT(container))
            return;

        container->mergeWithDescriptors(std::forward<Descriptors>(descriptors), scopes...);
    }

private:
    DescriptorMap mergedDescriptorsInternal()
    {
        MergeExecutor mergeExecutor;
        DescriptorMap result;
        for (const auto& [resourceId, container]: m_containers)
        {
            auto descriptors = container->descriptors();
            if (!descriptors)
                continue;

            MapHelper::merge(&result, *descriptors, mergeExecutor);
        }

        return result;
    }

    Container* ownResourceContainer()
    {
        auto itr = m_containers.find(m_ownResourceId);
        if (itr == m_containers.cend())
            return nullptr;

        return itr->second.get();
    }

private:
    QnUuid m_ownResourceId;
    std::map<QnUuid, std::unique_ptr<Container>> m_containers;

    mutable QnMutex m_mutex;
    CachedValue<DescriptorMap> m_cachedMergedDescriptors;
};

} // namespace nx::analytics
