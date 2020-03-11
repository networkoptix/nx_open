#pragma once

#include <nx/vms/metadata/metadata_archive.h>
#include <nx/vms/metadata/metadata_helper.h>
#include <recording/time_period_list.h>
#include <api/helpers/chunks_request_data.h>

#include "analytics_archive.h"

namespace nx::analytics::db {

class AnalyticsHelper:
    public nx::vms::metadata::MetadataHelper
{
    using base_type = nx::vms::metadata::MetadataHelper;

public:
    using base_type::base_type;

    QnTimePeriodList matchImage(
        const QnSecurityCamResourceList& resourceList,
        const AnalyticsArchive::AnalyticsFilter& filter);
};

} // namespace nx::analytics::db
