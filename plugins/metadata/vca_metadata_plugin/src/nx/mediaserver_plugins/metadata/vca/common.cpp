#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

bool operator==(const EventType& lhs, const EventType& rhs)
{
    return lhs.id == rhs.id;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json),
    VcaEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginManifest, (json),
    VcaPluginManifest_Fields, (brief, true))

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugin
} // namespace nx
