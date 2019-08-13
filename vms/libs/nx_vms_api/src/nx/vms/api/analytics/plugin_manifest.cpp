#include "plugin_manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginManifest, (json),
    nx_vms_api_analytics_PluginManifest_Fields, (brief, true))

std::vector<ManifestError> validate(const PluginManifest& pluginManifest)
{
    std::vector<ManifestError> result;
    if (pluginManifest.id.isEmpty())
        result.emplace_back(ManifestErrorType::emptyPluginId);

    if (pluginManifest.name.isEmpty())
        result.emplace_back(ManifestErrorType::emptyPluginName);

    if (pluginManifest.description.isEmpty())
        result.emplace_back(ManifestErrorType::emptyPluginDescription);

    if (pluginManifest.version.isEmpty())
        result.emplace_back(ManifestErrorType::emptyPluginVersion);

    if (pluginManifest.vendor.isEmpty())
        result.emplace_back(ManifestErrorType::emptyPluginVendor);

    return result;
}

} // namespace nx::vms::api::analytics
