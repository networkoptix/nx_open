#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dw_tvt {

bool operator==(const EventType& lh, const EventType& rh)
{
    return lh.id == rh.id;
}

bool EngineManifest::supportsModel(const QString& model) const noexcept
{
    if (supportedCameraModels.isEmpty())
        return true;

    return std::find(supportedCameraModels.cbegin(), supportedCameraModels.cend(), model)
        != supportedCameraModels.cend();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json),
    DwTvtEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json),
    DwTvtEngineManifest_Fields, (brief, true))

} // namespace dw_tvt
} // namespace analytics
} // namespace mediaserver_plugin
} // namespace nx
