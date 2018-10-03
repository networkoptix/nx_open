#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

bool operator==(const EventType& lh, const EventType& rh)
{
    return lh.id == rh.id;
}

bool PluginManifest::supportsModel(const QString& model) const noexcept
{
    if (supportedCameraModels.isEmpty())
        return true;

    return std::find(supportedCameraModels.cbegin(), supportedCameraModels.cend(), model)
        != supportedCameraModels.cend();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json),
    DwMttEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginManifest, (json),
    DwMttPluginManifest_Fields, (brief, true))

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugin
} // namespace nx
