// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_service_type_display_helper.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/saas_data.h>

namespace nx::vms::common::saas {

using namespace nx::vms::api;

QString ServiceTypeDisplayHelper::serviceTypeDisplayString(const QString& serviceType)
{
    if (serviceType == SaasService::kLocalRecordingServiceType)
        return tr("Local recording");

    if (serviceType == SaasService::kAnalyticsIntegrationServiceType)
        return tr("Integration");

    if (serviceType == SaasService::kCloudRecordingType)
        return tr("Cloud storage");

    if (serviceType == SaasService::kLiveViewServiceType)
        return tr("Live view");
    NX_ASSERT(false, "No localized display name for unexpected SaaS service type");
    return serviceType;
}

} // nx::vms::common::saas
