// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_resource_processor.h"

#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>

void QnClientResourceProcessor::processResources(const QnResourceList& resources)
{
    qnClientCoreModule->resourcePool()->addResources(resources);
}
