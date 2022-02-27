// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine_descriptor_manager.h"

#include <nx/analytics/utils.h>
#include <nx/analytics/properties.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

EngineDescriptorManager::EngineDescriptorManager(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
}

std::optional<EngineDescriptor> EngineDescriptorManager::descriptor(const EngineId& engineId) const
{
    return fetchDescriptor<EngineDescriptor>(commonModule()->descriptorContainer(), engineId);
}

EngineDescriptorMap EngineDescriptorManager::descriptors(const std::set<EngineId>& engineIds) const
{
    return fetchDescriptors<EngineDescriptor>(
        commonModule()->descriptorContainer(),
        engineIds);
}

} // namespace nx::analytics
