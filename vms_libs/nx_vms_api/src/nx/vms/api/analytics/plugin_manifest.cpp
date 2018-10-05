#include "plugin_manifest.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics::PluginManifest, Capability)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics::PluginManifest, Capabilities)

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginManifest::ObjectAction, (json),
    ObjectAction_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginManifest, (json),
    PluginManifest_Fields, (brief, true))

} // namespace nx::vms::api::analytics
