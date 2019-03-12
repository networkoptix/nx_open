#include "settings_response.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    SettingsResponse,
    (json)(eq),
    nx_vms_api_analytics_SettingsResponse_Fields,
    (brief, true))

} // namespace nx::vms::api::analytics
