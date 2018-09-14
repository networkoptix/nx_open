#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

bool operator==(const AnalyticsEventType& lh, const AnalyticsEventType& rh)
{
    return lh.id == rh.id;
}

bool AnalyticsDriverManifest::supportsModel(const QString& model) const noexcept
{
    if (supportedCameraModels.isEmpty())
        return true;

    return std::find(supportedCameraModels.cbegin(), supportedCameraModels.cend(), model)
        != supportedCameraModels.cend();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsEventType, (json),
    DwMttAnalyticsEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsDriverManifest, (json),
    DwMttAnalyticsDriverManifest_Fields, (brief, true))

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugin
} // namespace nx
