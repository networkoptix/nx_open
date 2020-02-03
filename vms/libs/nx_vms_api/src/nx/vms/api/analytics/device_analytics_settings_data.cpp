#include "device_analytics_settings_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceAnalyticsSettingsRequest,
    (json)(eq),
    nx_vms_api_analytics_DeviceAnalyticsSettingsRequest_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceAnalyticsSettingsResponse,
    (json)(eq),
    nx_vms_api_analytics_DeviceAnalyticsSettingsResponse_Fields,
    (brief, true))

} // namespace nx::vms::api::analytics
