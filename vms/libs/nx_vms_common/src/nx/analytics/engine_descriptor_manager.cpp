// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine_descriptor_manager.h"

#include <nx/analytics/utils.h>
#include <nx/analytics/properties.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

EngineDescriptorManager::EngineDescriptorManager(
    taxonomy::DescriptorContainer* taxonomyDescriptorContainer,
    QObject* parent)
    :
    base_type(parent),
    m_taxonomyDescriptorContainer(taxonomyDescriptorContainer)
{
}

std::optional<EngineDescriptor> EngineDescriptorManager::descriptor(const EngineId& engineId) const
{
    return fetchDescriptor<EngineDescriptor>(m_taxonomyDescriptorContainer, engineId);
}

EngineDescriptorMap EngineDescriptorManager::descriptors(const std::set<EngineId>& engineIds) const
{
    return fetchDescriptors<EngineDescriptor>(
        m_taxonomyDescriptorContainer,
        engineIds);
}

} // namespace nx::analytics
