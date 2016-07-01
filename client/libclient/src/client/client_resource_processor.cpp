#include "client_resource_processor.h"

#include <core/resource_management/resource_pool.h>

void QnClientResourceProcessor::processResources(const QnResourceList &resources) {
    qnResPool->addResources(resources);
}
