// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_resource_processor.h"

#include <core/resource_management/resource_pool.h>

QnClientResourceProcessor::QnClientResourceProcessor(
    nx::vms::common::SystemContext* systemContext)
    :
    nx::vms::common::SystemContextAware(systemContext)
{
}

void QnClientResourceProcessor::processResources(const QnResourceList& resources)
{
    resourcePool()->addResources(resources);
}
