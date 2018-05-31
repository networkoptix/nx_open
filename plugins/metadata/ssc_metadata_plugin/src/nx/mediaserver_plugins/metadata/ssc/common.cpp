#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace ssc {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsEventType, (json),
    SscAnalyticsEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsDriverManifest, (json),
    SscAnalyticsDriverManifest_Fields, (brief, true))

} // namespace ssc
} // namespace metadata
} // namespace mediaserver_plugin
} // namespace nx
