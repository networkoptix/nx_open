#pragma once

#include <nx/analytics/descriptor_container.h>
#include <nx/analytics/property_descriptor_storage.h>
#include <nx/analytics/multiresource_descriptor_container_helper.h>
#include <core/resource_management/resource_pool.h>
#include <utils/common/value_cache.h>
#include <common/common_module_aware.h>

namespace nx::analytics {

template<typename StorageFactoryType, typename MergeExecutorType>
class MultiresourceDescriptorContainer: public QnCommonModuleAware
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
    MultiresourceDescriptorContainer(QnCommonModule* commonModule, StorageFactory storageFactory):
        QnCommonModuleAware(commonModule),
        m_storageFactory(std::move(storageFactory)),
        m_helper(
            commonModule->resourcePool(),
            [this](QnMediaServerResourcePtr server) { addServerContainer(server); },
            [this](QnMediaServerResourcePtr server) { removeServerContainer(server); }),
        m_cachedMergedDescriptors(
            [this]() { return mergedDescriptorsInternal(); },
            &m_mutex)
    {
    }

    /**
     * Returns descriptors from all container resources merged into a single map with the help of
     * merge executor.
     */
    std::optional<TopLevelMap> descriptors() const
    {
        TopLevelMap result;

        std::map<QnUuid, std::shared_ptr<Container>> containers;
        {
            QnMutexLocker lock(&m_mutex);
            containers = m_containers;
        }

        for (const auto&[resourceId, container]: containers)
        {
            if (!NX_ASSERT(container))
                continue;

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
        std::shared_ptr<Container> container;
        {
            QnMutexLocker lock(&m_mutex);
            auto itr = m_containers.find(resourceId);
            if (itr == m_containers.cend())
                return std::nullopt;

            container = itr->second;
            if (!NX_ASSERT(container))
                return std::nullopt;
        }

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
        std::shared_ptr<Container> container;
        {
            QnMutexLocker lock(&m_mutex);
            container = ownResourceContainer();
            if (!NX_ASSERT(container))
                return;
        }

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
        std::shared_ptr<Container> container;
        {
            QnMutexLocker lock(&m_mutex);
            container = ownResourceContainer();
            if (!NX_ASSERT(container))
                return;
        }

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
        std::shared_ptr<Container> container;
        {
            QnMutexLocker lock(&m_mutex);
            container = ownResourceContainer();
            if (!NX_ASSERT(container))
                return;
        }

        container->mergeWithDescriptors(std::forward<Descriptors>(descriptors), scopes...);
    }

private:
    DescriptorMap mergedDescriptorsInternal()
    {
        MergeExecutor mergeExecutor;
        DescriptorMap result;

        std::map<QnUuid, std::shared_ptr<Container>> containers;
        {
            QnMutexLocker lock(&m_mutex);
            containers = m_containers;
        }

        for (const auto& [resourceId, container]: containers)
        {
            auto descriptors = container->descriptors();
            if (!descriptors)
                continue;

            MapHelper::merge(&result, *descriptors, mergeExecutor);
        }

        return result;
    }

    std::shared_ptr<Container> ownResourceContainer()
    {
        auto itr = m_containers.find(m_ownResourceId);
        if (itr == m_containers.cend())
            return nullptr;

        return itr->second;
    }

    void addServerContainer(QnMediaServerResourcePtr server)
    {
        QnMutexLocker lock(&m_mutex);
        const QnUuid serverId = server->getId();

        if (serverId == commonModule()->moduleGUID())
            m_ownResourceId = serverId;

        const auto notifyWhenUpdated = [this]() { m_cachedMergedDescriptors.reset(); };
        auto storage = m_storageFactory(server, notifyWhenUpdated);
        m_containers[serverId] = std::make_shared<Container>(std::move(storage));
        m_cachedMergedDescriptors.resetThreadUnsafe();
    }

    void removeServerContainer(QnMediaServerResourcePtr server)
    {
        QnMutexLocker lock(&m_mutex);
        const QnUuid serverId = server->getId();

        if (serverId == commonModule()->moduleGUID())
            m_ownResourceId = QnUuid();

        m_containers.erase(serverId);
        m_cachedMergedDescriptors.resetThreadUnsafe();
    }

private:
    QnUuid m_ownResourceId;
    std::map<QnUuid, std::shared_ptr<Container>> m_containers;


    StorageFactory m_storageFactory;
    MultiresourceDescriptorContainerHelper m_helper;

    mutable QnMutex m_mutex;
    CachedValue<DescriptorMap> m_cachedMergedDescriptors;
};

} // namespace nx::analytics
