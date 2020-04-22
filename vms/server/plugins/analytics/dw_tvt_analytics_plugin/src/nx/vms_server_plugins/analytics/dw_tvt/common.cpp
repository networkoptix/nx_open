#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms_server_plugins::analytics::dw_tvt {

bool EngineManifest::supportsModel(const QString& model) const noexcept
{
    if (supportedCameraModels.isEmpty())
        return true;

    const auto modelIt = std::find_if(supportedCameraModels.cbegin(), supportedCameraModels.cend(),
        [&model](const auto& supportedModel) { return model.startsWith(supportedModel); });

    return modelIt != supportedCameraModels.cend();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (eq)(json), DwTvtEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json), DwTvtEngineManifest_Fields, (brief, true))

} // nx::vms_server_plugins::analytics::dw_tvt
