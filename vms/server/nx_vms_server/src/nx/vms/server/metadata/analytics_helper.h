#pragma once

#include "metadata_helper.h"
#include <recording/time_period_list.h>
#include <api/helpers/chunks_request_data.h>
#include "analytics_archive.h"

namespace nx::vms::server::metadata {

class AnalyticsHelper: public MetadataHelper
{
public:
    using MetadataHelper::MetadataHelper;
    QnTimePeriodList matchImage(
        const QnSecurityCamResourceList& resourceList,
        const AnalyticsArchive::AnalyticsFilter& filter);
};

} // namespace nx::vms::server::metadata
