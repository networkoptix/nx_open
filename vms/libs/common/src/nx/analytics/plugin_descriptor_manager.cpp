#include "plugin_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

namespace {

const QString kPluginDescriptorTypeName("Plugin");

} // namespace

PluginDescriptorManager::PluginDescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_pluginDescriptorContainer(
        makeContainer<PluginDescriptorContainer>(commonModule, kPluginDescriptorsProperty))
{
}

std::optional<PluginDescriptor> PluginDescriptorManager::descriptor(const PluginId& pluginId) const
{
    return fetchDescriptor(m_pluginDescriptorContainer, pluginId);
}

PluginDescriptorMap PluginDescriptorManager::descriptors(const std::set<PluginId>& pluginIds) const
{
    return fetchDescriptors(m_pluginDescriptorContainer, pluginIds, kPluginDescriptorTypeName);
}

void PluginDescriptorManager::updateFromPluginManifest(const PluginManifest& manifest)
{
    PluginDescriptor descriptor{manifest.id, manifest.name};
    m_pluginDescriptorContainer.mergeWithDescriptors(std::move(descriptor), manifest.id);
}

} // namespace nx::analytics
