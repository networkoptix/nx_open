#include "plugin_manifest_base.h"

#include <nx/fusion/model_functions.h>

namespace nx::mediaserver_plugins::utils {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginManifestBase, (json),
    PluginManifestBase_Fields, (brief, true))

} // namespace nx::mediaserver_plugins::utils
