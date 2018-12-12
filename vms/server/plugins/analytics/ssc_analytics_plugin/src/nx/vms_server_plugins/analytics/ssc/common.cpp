#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace ssc {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json),
    SscEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json),
    SscEngineManifest_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AllowedPortNames, (json),
    AllowedPortNames_Fields, (brief, true))

} // namespace ssc
} // namespace analytics
} // namespace mediaserver_plugin
} // namespace nx
