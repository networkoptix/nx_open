#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx::mediaserver_plugins::utils {

/**
 * Plugin manifest fragment common to many plugins. When serialized, produces a compatible subset
 * of a complete plugin manifest. Intended to be inherited and extended with plugin-proprietary
 * fields. Such fields may also appear inside compound fields, and for this purpose those compound
 * fields are not included in this class to be able to be implemented using classes derived from
 * the original field types.
 */
struct NX_PLUGIN_UTILS_API PluginManifestBase
{
    QString pluginId;
    nx::vms::api::analytics::TranslatableString pluginName;
    nx::vms::api::analytics::PluginManifest::Capabilities capabilities;
};
#define PluginManifestBase_Fields (pluginId)(pluginName)(capabilities)
QN_FUSION_DECLARE_FUNCTIONS(PluginManifestBase, (json), NX_PLUGIN_UTILS_API)

} // namespace nx::mediaserver_plugins::utils
