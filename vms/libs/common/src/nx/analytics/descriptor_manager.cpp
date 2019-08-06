#include "descriptor_manager.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

DescriptorManager::DescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

void DescriptorManager::updateFromPluginManifest(
    const nx::vms::api::analytics::PluginManifest& manifest)
{
    commonModule()->analyticsPluginDescriptorManager()->updateFromPluginManifest(manifest);

    commit();
}

void DescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const nx::vms::api::analytics::EngineManifest& manifest)
{
    commonModule()->analyticsEngineDescriptorManager()->updateFromEngineManifest(
        pluginId, engineId, engineName,  manifest);
    commonModule()->analyticsGroupDescriptorManager()->updateFromEngineManifest(
        pluginId, engineId, engineName, manifest);

    commonModule()->analyticsEventTypeDescriptorManager()->updateFromEngineManifest(
        pluginId, engineId, engineName, manifest);
    commonModule()->analyticsObjectTypeDescriptorManager()->updateFromEngineManifest(
        pluginId, engineId, engineName, manifest);

    commit();
}

void DescriptorManager::updateFromDeviceAgentManifest(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    commonModule()->analyticsGroupDescriptorManager()->updateFromDeviceAgentManifest(
        deviceId, engineId, manifest);
    commonModule()->analyticsEventTypeDescriptorManager()->updateFromDeviceAgentManifest(
        deviceId, engineId, manifest);
    commonModule()->analyticsObjectTypeDescriptorManager()->updateFromDeviceAgentManifest(
        deviceId, engineId, manifest);

    commit();
}

void DescriptorManager::commit()
{
    const auto resPool = resourcePool();
    if (!resPool)
        return;

    const auto commonModule = this->commonModule();
    if (!commonModule)
        return;

    const auto server = resPool->getResourceById<QnMediaServerResource>(
        commonModule->moduleGUID());

    if (server)
        server->savePropertiesAsync();
}

} // namespace nx::analytics
