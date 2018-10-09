#include "analytics_engine_resource.h"

namespace nx::vms::common {

AnalyticsEngineResource::AnalyticsEngineResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

AnalyticsEngineResource::~AnalyticsEngineResource() = default;

} // namespace nx::vms::common
