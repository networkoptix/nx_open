#include "driver_manifest.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::api::AnalyticsDriverManifestBase, Capability)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::api::AnalyticsDriverManifestBase, Capabilities)

namespace nx {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsDriverManifest, (json), AnalyticsDriverManifest_Fields,
    (brief, true))

} // namespace api
} // namespace nx
