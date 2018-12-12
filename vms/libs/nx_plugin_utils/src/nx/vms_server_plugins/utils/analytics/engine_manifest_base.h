#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::vms_server_plugins::utils::analytics {

/**
 * Engine manifest fragment common to many plugins. When serialized, produces a compatible subset
 * of a complete Engine manifest. Intended to be inherited and extended with Engine-proprietary
 * fields. Such fields may also appear inside compound fields, and for this purpose those compound
 * fields are not included in this class to be able to be implemented using classes derived from
 * the original field types.
 */
struct NX_PLUGIN_UTILS_API EngineManifestBase
{
    nx::vms::api::analytics::EngineManifest::Capabilities capabilities;
};
#define EngineManifestBase_Fields (capabilities)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifestBase, (json), NX_PLUGIN_UTILS_API)

} // namespace nx::vms_server_plugins::utils::analytics
