#pragma once

#include <nx/utils/data_structures/map_helper.h>
#include <nx/utils/std/optional.h>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/model_functions.h>

#include <nx/analytics/property_descriptor_storage.h>
#include <nx/analytics/default_descriptor_merger.h>

namespace nx::analytics {

using namespace nx::utils::data_structures;

template<
    typename DescriptorStorage,
    typename DescriptorMerger = DefaultDescriptorMerger<typename DescriptorStorage::Descriptor>
>
class DescriptorContainer
{
    using Descriptor = typename DescriptorStorage::Descriptor;
    using Container = typename DescriptorStorage::Container;

public:
    DescriptorContainer(
        std::unique_ptr<DescriptorStorage> descriptorStorage,
        DescriptorMerger descriptorMerger = DescriptorMerger())
        :
        m_descriptorStorage(std::move(descriptorStorage)),
        m_descriptorMerger(std::move(descriptorMerger))
    {
    }

    template<typename... Scopes>
    auto descriptors(const Scopes&... scopes)
        -> std::optional<MapHelper::MappedTypeOnLevel<Container, sizeof...(Scopes)>>
    {
        const auto allDescriptors = m_descriptorStorage->fetch();
        return MapHelper::getOptional(allDescriptors, scopes...);
    }

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

    template<typename Descriptors, typename... Scopes>
    void setDescriptors(Descriptors&& descriptors, const Scopes&... scopes)
    {
        auto allDescriptors = m_descriptorStorage->fetch();
        MapHelper::set(&allDescriptors, scopes..., std::forward<Descriptors>(descriptors));
        m_descriptorStorage->save(std::move(allDescriptors));
    }

    template<typename Descriptors, typename... Scopes>
    void addDescriptors(const Descriptors& descriptors, const Scopes&... scopes)
    {
        auto allDescriptors = m_descriptorStorage->fetch();
        const auto currentDescriptors = MapHelper::get(allDescriptors, scopes...);

        if constexpr (MapHelper::mapDepth<Container>() == sizeof...(Scopes))
        {
            const auto merged = m_descriptorMerger(&currentDescriptors, &descriptors);
            if (merged)
                MapHelper::set(&allDescriptors, scopes..., *merged);
        }
        else
        {
            const auto merged = MapHelper::merge(
                currentDescriptors, descriptors, m_descriptorMerger);
            MapHelper::set(&allDescriptors, scopes..., merged);
        }

        // TODO: #dmishin Test this stuff.
        m_descriptorStorage->save(std::move(allDescriptors));
    }

private:
    std::unique_ptr<DescriptorStorage> m_descriptorStorage;
    DescriptorMerger m_descriptorMerger;
};

} // namespace nx::analytics
