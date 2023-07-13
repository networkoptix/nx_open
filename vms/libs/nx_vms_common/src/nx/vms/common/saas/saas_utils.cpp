// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_utils.h"

#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::common::saas {

bool saasIsActive(SystemContext* context)
{
    return context->saasServiceManager()->data().state == nx::vms::api::SaasState::active;
}

} // nx::vms::common::saas

