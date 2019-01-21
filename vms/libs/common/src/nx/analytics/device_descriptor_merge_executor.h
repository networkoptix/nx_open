#pragma once

#include <optional>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics {

class DeviceDescriptorMergeExecutor
{
public:
    std::optional<nx::vms::api::analytics::DeviceDescriptor> operator()(
        const nx::vms::api::analytics::DeviceDescriptor* first,
        const nx::vms::api::analytics::DeviceDescriptor* second) const;
};

} // namespace nx::analytics
