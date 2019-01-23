#include "utils.h"

namespace nx::analytics {

using namespace nx::vms::api::analytics;

std::set<QString> supportedEventTypeIdsFromManifest(const DeviceAgentManifest& manifest)
{
    std::set<QString> result(
        manifest.supportedEventTypeIds.begin(),
        manifest.supportedEventTypeIds.end());

    for (const auto& eventType: manifest.eventTypes)
        result.insert(eventType.id);

    return result;
}

std::set<QString> supportedObjectTypeIdsFromManifest(const DeviceAgentManifest& manifest)
{
    std::set<QString> result(
        manifest.supportedObjectTypeIds.begin(),
        manifest.supportedObjectTypeIds.end());

    for (const auto& objectType: manifest.objectTypes)
        result.insert(objectType.id);

    return result;
}

std::map<EngineId, std::set<EventTypeId>> eventTypeIdsSupportedByDevice(
    const DeviceDescriptorContainer& container,
    const DeviceId& deviceId)
{
    auto descriptor = container.mergedDescriptors(deviceId);
    if (!descriptor)
        return {};

    return descriptor->supportedEventTypeIds;
}

std::map<EngineId, std::set<ObjectTypeId>> objectTypeIdsSupportedByDevice(
    const DeviceDescriptorContainer& container,
    const DeviceId& deviceId)
{
    auto descriptor = container.mergedDescriptors(deviceId);
    if (!descriptor)
        return {};

    return descriptor->supportedObjectTypeIds;
}

} // namespace nx::analytics
