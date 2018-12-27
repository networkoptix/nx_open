#include "device_descriptor_merge_executor.h"

namespace nx::analytics {

using namespace nx::vms::api::analytics;

std::optional<DeviceDescriptor> DeviceDescriptorMergeExecutor::operator()(
    const DeviceDescriptor* first,
    const DeviceDescriptor* second) const
{
    if (!first && !second)
        return std::nullopt;

    if (!first)
        return *second;

    if (!second)
        return *first;

    auto mergeResult = *first;

    const auto& additionalCompatibleEngines = second->compatibleEngineIds;
    mergeResult.compatibleEngineIds.insert(
        additionalCompatibleEngines.cbegin(),
        additionalCompatibleEngines.cend());

    return mergeResult;
}

} // namespace nx::analytics
