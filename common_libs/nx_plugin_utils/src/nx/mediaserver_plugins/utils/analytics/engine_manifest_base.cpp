#include "engine_manifest_base.h"

#include <nx/fusion/model_functions.h>

namespace nx::mediaserver_plugins::utils::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifestBase, (json),
    EngineManifestBase_Fields, (brief, true))

} // namespace nx::mediaserver_plugins::utils::analytics
