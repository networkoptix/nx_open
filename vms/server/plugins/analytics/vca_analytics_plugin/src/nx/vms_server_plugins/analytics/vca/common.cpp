#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace vca {

bool operator==(const EventType& lhs, const EventType& rhs)
{
    return lhs.id == rhs.id;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json),
    VcaEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json),
    VcaEngineManifest_Fields, (brief, true))

} // namespace vca
} // namespace analytics
} // namespace mediaserver_plugin
} // namespace nx
