#pragma once

#include <nx/utils/data_structures/map_helper.h>
#include <nx/utils/std/optional.h>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/model_functions.h>

#include <nx/analytics/property_descriptor_storage.h>
#include <nx/analytics/replacement_merge_executor.h>

namespace nx::analytics {

using namespace nx::utils::data_structures;

template<
    typename DescriptorStorage,
    typename MergeExecutor = ReplacementMergeExecutor<typename DescriptorStorage::Descriptor>
>
class DescriptorContainer
{
    using Descriptor = typename DescriptorStorage::Descriptor;
    using Container = typename DescriptorStorage::Container;

public:
    DescriptorContainer(
        std::unique_ptr<DescriptorStorage> descriptorStorage,
        MergeExecutor mergeExecutor = MergeExecutor())
        :
        m_descriptorStorage(std::move(descriptorStorage)),
        m_mergeExecutor(std::move(mergeExecutor))
    {
    }

    /**
     * Returns descriptors corresponding to the provided scopes.
     */
    template<typename... Scopes>
    auto descriptors(const Scopes&... scopes) const
        -> std::optional<MapHelper::MappedTypeOnLevel<Container, sizeof...(Scopes)>>
    {
        const auto allDescriptors = m_descriptorStorage->fetch();
        return MapHelper::getOptional(allDescriptors, scopes...);
    }

    /**
     * Removes descriptors by the specified scope chain.
     */
    void removeDescriptors()
    {
        m_descriptorStorage->save(Container());
    }

    template<typename... Scopes>
    void removeDescriptors(const Scopes&... scopes)
    {
        auto allDescriptors = m_descriptorStorage->fetch();
        MapHelper::erase(&allDescriptors, scopes...);
        m_descriptorStorage->save(std::move(allDescriptors));
    }

    /**
     * Inserts or replaces descriptors by the specified scope chain.
     */
    template<typename Descriptors, typename... Scopes>
    void setDescriptors(Descriptors&& descriptors, const Scopes&... scopes)
    {
        auto allDescriptors = m_descriptorStorage->fetch();
        MapHelper::set(&allDescriptors, scopes..., std::forward<Descriptors>(descriptors));
        m_descriptorStorage->save(std::move(allDescriptors));
    }

    /**
     * Merges provided descriptors with the current ones with the help of merge executor.
     */
    template<typename Descriptors, typename... Scopes>
    void mergeWithDescriptors(const Descriptors& descriptors, const Scopes&... scopes)
    {
        auto allDescriptors = m_descriptorStorage->fetch();
        const auto currentDescriptors = MapHelper::get(allDescriptors, scopes...);

        if constexpr (MapHelper::mapDepth<Container>() == sizeof...(Scopes))
        {
            const auto merged = m_mergeExecutor(&currentDescriptors, &descriptors);
            if (merged)
                MapHelper::set(&allDescriptors, scopes..., *merged);
        }
        else
        {
            const auto merged = MapHelper::merge(
                currentDescriptors, descriptors, m_mergeExecutor);
            MapHelper::set(&allDescriptors, scopes..., merged);
        }

        m_descriptorStorage->save(std::move(allDescriptors));
    }

private:
    std::unique_ptr<DescriptorStorage> m_descriptorStorage;
    MergeExecutor m_mergeExecutor;
};

} // namespace nx::analytics
