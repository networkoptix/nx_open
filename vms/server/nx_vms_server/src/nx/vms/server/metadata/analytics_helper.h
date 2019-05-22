#pragma once

#include "metadata_helper.h"
#include <recording/time_period_list.h>
#include <api/helpers/chunks_request_data.h>


namespace nx::vms::server::metadata {

class AnalyticsHelper: public MetadataHelper
{
public:
    using MetadataHelper::MetadataHelper;
    QnTimePeriodList matchImage(const QnChunksRequestData& request);
};

} // namespace nx::vms::server::metadata
