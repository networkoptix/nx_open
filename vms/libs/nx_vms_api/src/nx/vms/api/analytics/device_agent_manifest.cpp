#include "device_agent_manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceAgentManifest, (json), DeviceAgentManifest_Fields,
    (brief, true))

} // namespace nx::vms::api::analytics
