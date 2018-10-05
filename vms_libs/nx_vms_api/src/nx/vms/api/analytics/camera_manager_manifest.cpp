#include "camera_manager_manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraManagerManifest, (json), CameraManagerManifest_Fields,
    (brief, true))

} // namespace nx::vms::api::analytics
