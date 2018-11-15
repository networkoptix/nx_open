#include "plugin_manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginManifest, (json),
    nx_vms_api_analytics_PluginManifest_Fields, (brief, true))

} // namespace nx::vms::api::analytics
