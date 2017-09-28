#include "driver_manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsDriverManifest, (json), AnalyticsDriverManifest_Fields,
    (brief, true))

} // namespace api
} // namespace nx
