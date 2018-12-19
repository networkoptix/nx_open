#pragma once

#include <nx/analytics/descriptor_container.h>
#include <nx/analytics/property_descriptor_storage.h>

namespace nx::analytics {

template<typename StorageFactory, typename Merger>
class MultiresourceDescriptorContainer
{
    using Container = DescriptorContainer<
        typename decltype(std::declval<StorageFactory>()(QnResourcePtr()))::element_type,
        Merger>;

    using DescriptorMap = typename decltype(std::declval<Container>().descriptors())::value_type;
    using TopLevelMap = typename MapHelper::Wrapper<DescriptorMap>::template wrap<QnUuid>;

public:
    MultiresourceDescriptorContainer(
        StorageFactory storageFactory,
        const QnResourceList& resourceList,
        QnResourcePtr ownResource)
    {
        for (auto& resource: resourceList)
        {
            auto storage = storageFactory(resource);
            m_containers[resource->getId()] = std::make_unique<Container>(std::move(storage));
        }

        if (!ownResource)
            return;

        auto writableStorage = storageFactory(ownResource);
        m_ownResourceId = ownResource->getId();
        m_containers[m_ownResourceId] =
            std::make_unique<Container>(std::move(writableStorage));
    }

    std::optional<TopLevelMap> descriptors()
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

    template<typename... Scopes>
    auto descriptors(const QnUuid& resourceId, const Scopes&... scopes)
        -> std::optional<MapHelper::MappedTypeOnLevel<DescriptorMap, sizeof...(Scopes)>>
    {
        auto itr = m_containers.find(resourceId);
        if (itr == m_containers.cend())
            return std::nullopt;

        auto& container = itr->second;
        return container->descriptors(scopes...);
    }

    template<typename... Scopes>
    auto mergedDescriptors(const Scopes&... scopes)
        ->std::optional<MapHelper::MappedTypeOnLevel<DescriptorMap, sizeof...(Scopes)>>
    {
        Merger merger;
        std::optional<MapHelper::MappedTypeOnLevel<DescriptorMap, sizeof...(Scopes)>> result;
        for (const auto& [resourceId, container]: m_containers)
        {
            auto descriptors = container->descriptors(scopes...);
            if (!descriptors)
                continue;

            if (!result)
            {
                result = descriptors;
                continue;
            }

            if constexpr (MapHelper::isMap(*descriptors))
                MapHelper::merge(&*result, *descriptors, merger);
            else
                result = merger(&*result, &*descriptors);
        }

        if (!result)
            return std::nullopt;

        return result;
    }

    // Methods below affect only own resource.
    template<typename... Scopes>
    void removeDescriptors(const Scopes&... scopes)
    {
        auto container = ownResourceContainer();
        if (!NX_ASSERT(container))
            return;

        container->removeDescriptors(scopes...);
    }

    template<typename Descriptors, typename... Scopes>
    void setDescriptors(Descriptors&& descriptors, const Scopes&... scopes)
    {
        auto container = ownResourceContainer();
        if (!NX_ASSERT(container))
            return;

        container->setDescriptors(std::forward<Descriptors>(descriptors), scopes...);
    }

    template<typename Descriptors, typename... Scopes>
    void addDescriptors(Descriptors&& descriptors, const Scopes&... scopes)
    {
        auto container = ownResourceContainer();
        if (!NX_ASSERT(container))
            return;

        container->addDescriptors(std::forward<Descriptors>(descriptors), scopes...);
    }

private:
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
};

} // namespace nx::analytics
