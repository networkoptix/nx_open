#pragma once

#include <QElapsedTimer>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace ssc { //< SSC = Self Service Console

/** Description of the SSC analytics event. */
struct EventType: nx::vms::api::analytics::EventType
{
    // SSC-camera event type name: "camera specify command" or "reset command".
    QString internalName;
};
#define SscEventType_Fields EventType_Fields(internalName)

struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
{
    QString serialPortName; //< Proprietary.
    QList<EventType> eventTypes;
};
#define SscEngineManifest_Fields EngineManifestBase_Fields (serialPortName)(eventTypes)

struct AllowedPortNames
{
    bool useAllPorts = true;
    QList<QString> values;
};
#define AllowedPortNames_Fields (useAllPorts)(values)

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json))
QN_FUSION_DECLARE_FUNCTIONS(AllowedPortNames, (json))

} // namespace ssc
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
