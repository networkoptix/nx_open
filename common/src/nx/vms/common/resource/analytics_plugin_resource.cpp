#include "analytics_plugin_resource.h"

namespace nx::vms::common {

AnalyticsPluginResource::AnalyticsPluginResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

AnalyticsPluginResource::~AnalyticsPluginResource() = default;

} // namespace nx::vms::common
