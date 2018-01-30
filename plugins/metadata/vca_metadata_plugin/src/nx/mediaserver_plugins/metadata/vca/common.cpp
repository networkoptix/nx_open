#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {

bool operator==(const Vca::VcaAnalyticsEventType& lh, const Vca::VcaAnalyticsEventType& rh)
{
    return lh.eventTypeId == rh.eventTypeId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Vca::VcaAnalyticsEventType, (json),
    VcaAnalyticsEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Vca::VcaAnalyticsDriverManifest, (json),
    VcaAnalyticsDriverManifest_Fields, (brief, true))

} // namespace metadata
} // namespace mediaserver_plugin
} // namespace nx
