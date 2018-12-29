#include "device_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

namespace {

const QString kDeviceDescriptorTypeName("Device");

} // namespace

DeviceDescriptorManager::DeviceDescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_deviceDescriptorContainer(
        makeContainer<DeviceDescriptorContainer>(commonModule, kDeviceDescriptorsProperty))
{
}

std::optional<DeviceDescriptor> DeviceDescriptorManager::descriptor(const DeviceId& groupId) const
{
    return fetchDescriptor(m_deviceDescriptorContainer, groupId);
}

DeviceDescriptorMap DeviceDescriptorManager::descriptors(const std::set<DeviceId>& deviceIds) const
{
    return fetchDescriptors(m_deviceDescriptorContainer, deviceIds, kDeviceDescriptorTypeName);
}

void DeviceDescriptorManager::updateFromDeviceAgentManifest(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const DeviceAgentManifest& manifest)
{
    updateSupportedEventTypes(deviceId, engineId, manifest);
    updateSupportedObjectTypes(deviceId, engineId, manifest);
}

void DeviceDescriptorManager::clearRuntimeInfo()
{
    m_deviceDescriptorContainer.removeDescriptors();
}

void DeviceDescriptorManager::removeDeviceDescriptors(const std::set<DeviceId>& deviceIds)
{
    for (const auto& deviceId: deviceIds)
        m_deviceDescriptorContainer.removeDescriptors(deviceId);
}

void DeviceDescriptorManager::addCompatibleAnalyticsEngines(
    const DeviceId& deviceId,
    std::set<EngineId> compatibleEngineIds)
{
    DeviceDescriptor descriptor;
    descriptor.id = deviceId;
    descriptor.compatibleEngineIds = std::move(compatibleEngineIds);

    m_deviceDescriptorContainer.mergeWithDescriptors(descriptor, deviceId);
}

void DeviceDescriptorManager::removeCompatibleAnalyticsEngines(
    const DeviceId& deviceId,
    std::set<EngineId> engineIdsToRemove)
{
    if (engineIdsToRemove.empty())
        return;

    auto descriptor =
        m_deviceDescriptorContainer.descriptors(commonModule()->moduleGUID(), deviceId);

    if (!descriptor)
        return;

    for (const auto& engineId: engineIdsToRemove)
        descriptor->compatibleEngineIds.erase(engineId);

    m_deviceDescriptorContainer.setDescriptors(*descriptor, deviceId);
}

void DeviceDescriptorManager::removeCompatibleAnalyticsEngines(const std::set<EngineId>& engineIds)
{
    auto descriptors = m_deviceDescriptorContainer.descriptors(commonModule()->moduleGUID());
    if (!descriptors)
        descriptors = typename decltype(descriptors)::value_type();

    for (auto& [deviceId, descriptor]: *descriptors)
    {
        for (const auto& engineId: engineIds)
            descriptor.compatibleEngineIds.erase(engineId);
    }

    m_deviceDescriptorContainer.setDescriptors(*descriptors);
}

void DeviceDescriptorManager::setCompatibleAnalyticsEngines(
    const DeviceId& deviceId,
    std::set<EngineId> engineIds)
{
    auto descriptor = m_deviceDescriptorContainer.descriptors(
        commonModule()->moduleGUID(), deviceId);

    if (!descriptor)
    {
        DeviceDescriptor deviceDescriptor;
        deviceDescriptor.id = deviceId;
        descriptor = deviceDescriptor;
    }

    descriptor->compatibleEngineIds = std::move(engineIds);
    m_deviceDescriptorContainer.setDescriptors(*descriptor, deviceId);
}

std::set<EngineId> DeviceDescriptorManager::compatibleEngineIds(
    const QnVirtualCameraResourcePtr& device) const
{
    const auto descriptorForDevice = descriptor(device->getId());
    if (!descriptorForDevice)
        return {};

    return descriptorForDevice->compatibleEngineIds;
}

std::set<EngineId> DeviceDescriptorManager::compatibleEngineIdsUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::set<EngineId> result;
    for (const auto& device: devices)
    {
        const auto deviceCompatibleEngineIds = compatibleEngineIds(device);
        result.insert(deviceCompatibleEngineIds.cbegin(), deviceCompatibleEngineIds.cend());
    }

    return result;
}

std::set<EngineId> DeviceDescriptorManager::compatibleEngineIdsIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.isEmpty())
        return {};

    std::set<EngineId> result = compatibleEngineIds(devices[0]);
    for (auto i = 1; i < devices.size(); ++i)
    {
        std::set<EngineId> currentIntersection;
        const auto currentCompatibleEngineIds = compatibleEngineIds(devices[i]);
        std::set_intersection(
            result.cbegin(), result.cend(),
            currentCompatibleEngineIds.cbegin(), currentCompatibleEngineIds.cend(),
            std::inserter(currentIntersection, currentIntersection.end()));

        result.swap(currentIntersection);
    }

    return result;
}

DeviceDescriptor DeviceDescriptorManager::deviceDescriptor(const DeviceId& deviceId) const
{
    auto descriptorForDevice = m_deviceDescriptorContainer.mergedDescriptors(deviceId);
    if (!descriptorForDevice)
    {
        descriptorForDevice = nx::vms::api::analytics::DeviceDescriptor();
        descriptorForDevice->id = deviceId;
    }

    return *descriptorForDevice;
}

void DeviceDescriptorManager::updateSupportedEventTypes(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const DeviceAgentManifest& manifest)
{
    auto descriptorForDevice = deviceDescriptor(deviceId);

    descriptorForDevice.supportedEventTypeIds[engineId] =
        supportedEventTypeIdsFromManifest(manifest);

    m_deviceDescriptorContainer.setDescriptors(descriptorForDevice, deviceId);
}

void DeviceDescriptorManager::updateSupportedObjectTypes(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const DeviceAgentManifest& manifest)
{
    auto descriptorForDevice = deviceDescriptor(deviceId);

    descriptorForDevice.supportedObjectTypeIds[engineId] =
        supportedObjectTypeIdsFromManifest(manifest);

    m_deviceDescriptorContainer.setDescriptors(descriptorForDevice, deviceId);
}

} // namespace nx::analytics
