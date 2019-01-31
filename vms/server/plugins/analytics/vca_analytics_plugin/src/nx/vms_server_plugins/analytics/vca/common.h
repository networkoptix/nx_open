#pragma once

#include <QElapsedTimer>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace vca {

/** Description of the vca analytics event. */
struct EventType: nx::vms::api::analytics::EventType
{
    // VCA-camera event type name (this name is sent by VCA-camera tcp notification server).
    QString internalName;
};
#define VcaEventType_Fields EventType_Fields(internalName)

struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
{
    QList<EventType> eventTypes;
};
#define VcaEngineManifest_Fields EngineManifestBase_Fields (eventTypes)

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json))

bool operator==(const EventType& lhs, const EventType& rhs);

} // namespace vca
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
