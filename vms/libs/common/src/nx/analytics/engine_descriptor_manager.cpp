#include "engine_descriptor_manager.h"

#include <nx/analytics/utils.h>
#include <nx/analytics/properties.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

namespace {

const QString kEngineDescriptorTypeName("Engine");

} // namespace

EngineDescriptorManager::EngineDescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_engineDescriptorContainer(
        makeContainer<EngineDescriptorContainer>(commonModule, kEngineDescriptorsProperty))
{
}

std::optional<EngineDescriptor> EngineDescriptorManager::descriptor(const EngineId& engineId) const
{
    return fetchDescriptor(m_engineDescriptorContainer, engineId);
}

EngineDescriptorMap EngineDescriptorManager::descriptors(const std::set<EngineId>& engineIds) const
{
    return fetchDescriptors(m_engineDescriptorContainer, engineIds, kEngineDescriptorTypeName);
}

void EngineDescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const EngineManifest& manifest)
{
    m_engineDescriptorContainer.mergeWithDescriptors(
        EngineDescriptor(engineId, engineName, pluginId),
        engineId);
}

} // namespace nx::analytics
