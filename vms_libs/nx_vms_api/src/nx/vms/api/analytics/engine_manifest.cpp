#include "engine_manifest.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics::EngineManifest, Capability)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics::EngineManifest, Capabilities)

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest::ObjectAction, (json),
    ObjectAction_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json),
    EngineManifest_Fields, (brief, true))

} // namespace nx::vms::api::analytics
