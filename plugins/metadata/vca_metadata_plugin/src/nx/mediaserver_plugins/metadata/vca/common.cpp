#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

bool operator==(const VcaAnalyticsEventType& lh, const VcaAnalyticsEventType& rh)
{
    return lh.eventTypeId == rh.eventTypeId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VcaAnalyticsEventType, (json),
    VcaAnalyticsEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VcaAnalyticsDriverManifest, (json),
    VcaAnalyticsDriverManifest_Fields, (brief, true))

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugin
} // namespace nx
