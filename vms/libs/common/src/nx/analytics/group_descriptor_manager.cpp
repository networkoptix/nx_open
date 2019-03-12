#include "group_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

namespace {

const QString kGroupDescriptorTypeName("Engine");

} // namespace

GroupDescriptorManager::GroupDescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_groupDescriptorContainer(
        makeContainer<GroupDescriptorContainer>(commonModule, kGroupDescriptorsProperty))
{
}

std::optional<GroupDescriptor> GroupDescriptorManager::descriptor(
    const GroupId& groupId) const
{
    return fetchDescriptor(m_groupDescriptorContainer, groupId);
}

GroupDescriptorMap GroupDescriptorManager::descriptors(const std::set<GroupId>& groupIds) const
{
    return fetchDescriptors(m_groupDescriptorContainer, groupIds, kGroupDescriptorTypeName);
}

void GroupDescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const EngineManifest& manifest)
{
    m_groupDescriptorContainer.mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<GroupDescriptor>(engineId, manifest.groups));
}

void GroupDescriptorManager::updateFromDeviceAgentManifest(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const DeviceAgentManifest& manifest)
{
    m_groupDescriptorContainer.mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<GroupDescriptor>(engineId, manifest.groups));
}

} // namespace nx::analytics
