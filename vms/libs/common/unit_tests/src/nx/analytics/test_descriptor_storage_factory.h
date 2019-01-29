#pragma once

#include <core/resource/resource.h>

#include <nx/analytics/test_descriptor.h>
#include <nx/analytics/test_descriptor_storage.h>

namespace nx::analytics {

template<typename Descriptor, typename... Scopes>
class TestDescriptorStorageFactory
{
    using Storage = TestDescriptorStorage<Descriptor, Scopes...>;
public:
    std::unique_ptr<Storage> operator()(QnResourcePtr resource) const
    {
        return std::make_unique<Storage>();
    }
};

} // namespace nx::analytics
