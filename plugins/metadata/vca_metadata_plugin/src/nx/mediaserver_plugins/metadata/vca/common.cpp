#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

bool operator==(const AnalyticsEventType& lhs, const AnalyticsEventType& rhs)
{
    return lhs.id == rhs.id;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsEventType, (json),
    VcaAnalyticsEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsDriverManifest, (json),
    VcaAnalyticsDriverManifest_Fields, (brief, true))

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugin
} // namespace nx
