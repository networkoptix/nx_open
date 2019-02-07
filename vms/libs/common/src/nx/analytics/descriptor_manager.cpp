#include "descriptor_manager.h"

#include <common/common_module.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

using namespace nx::vms::api::analytics;

DescriptorManager::DescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_pluginDescriptorManager(commonModule),
    m_engineDescriptorManager(commonModule),
    m_groupDescriptorManager(commonModule),
    m_eventTypeDescriptorManager(commonModule),
    m_objectTypeDescriptorManager(commonModule),
    m_deviceDescriptorManager(commonModule)
{
}

void DescriptorManager::clearRuntimeInfo()
{
    m_deviceDescriptorManager.clearRuntimeInfo();
}

void DescriptorManager::updateFromPluginManifest(
    const nx::vms::api::analytics::PluginManifest& manifest)
{
    m_pluginDescriptorManager.updateFromPluginManifest(manifest);
}

void DescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const nx::vms::api::analytics::EngineManifest& manifest)
{
    m_engineDescriptorManager.updateFromEngineManifest(pluginId, engineId, engineName,  manifest);
    m_groupDescriptorManager.updateFromEngineManifest(pluginId, engineId, engineName, manifest);

    m_eventTypeDescriptorManager.updateFromEngineManifest(
        pluginId, engineId, engineName, manifest);
    m_objectTypeDescriptorManager.updateFromEngineManifest(
        pluginId, engineId, engineName, manifest);
}

void DescriptorManager::updateFromDeviceAgentManifest(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    m_groupDescriptorManager.updateFromDeviceAgentManifest(deviceId, engineId, manifest);
    m_eventTypeDescriptorManager.updateFromDeviceAgentManifest(deviceId, engineId, manifest);
    m_objectTypeDescriptorManager.updateFromDeviceAgentManifest(deviceId, engineId, manifest);
    m_deviceDescriptorManager.updateFromDeviceAgentManifest(deviceId, engineId, manifest);
}

} // namespace nx::analytics
