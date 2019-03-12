#pragma once

#include <nx/utils/data_structures/map_helper.h>

namespace nx::analytics {

using namespace nx::utils::data_structures;

template<typename DescriptorType, typename... Scopes>
class TestDescriptorStorage
{
public:
    using Descriptor = DescriptorType;
    using Container = MapHelper::NestedMap<std::map, Scopes..., DescriptorType>;

public:
    Container fetch() { return m_descriptors; }
    void save(Container descriptors) { m_descriptors = std::move(descriptors); }

private:
    Container m_descriptors;
};

} // namespace nx::analytics
