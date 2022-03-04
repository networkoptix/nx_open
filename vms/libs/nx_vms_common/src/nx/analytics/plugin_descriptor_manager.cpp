// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

PluginDescriptorManager::PluginDescriptorManager(
    taxonomy::DescriptorContainer* taxonomyDescriptorContainer,
    QObject* parent)
    :
    base_type(parent),
    m_taxonomyDescriptorContainer(taxonomyDescriptorContainer)
{
}

std::optional<PluginDescriptor> PluginDescriptorManager::descriptor(const PluginId& pluginId) const
{
    return fetchDescriptor<PluginDescriptor>(m_taxonomyDescriptorContainer, pluginId);
}

PluginDescriptorMap PluginDescriptorManager::descriptors(const std::set<PluginId>& pluginIds) const
{
    return fetchDescriptors<PluginDescriptor>(
        m_taxonomyDescriptorContainer,
        pluginIds);
}

} // namespace nx::analytics
