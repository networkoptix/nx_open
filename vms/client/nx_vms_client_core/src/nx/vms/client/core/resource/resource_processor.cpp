// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_processor.h"

#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::core {

ResourceProcessor::ResourceProcessor(common::SystemContext* systemContext):
    common::SystemContextAware(systemContext)
{
}

void ResourceProcessor::processResources(const QnResourceList& resources)
{
    resourcePool()->addResources(resources);
}

} // namespace nx::vms::client::core
