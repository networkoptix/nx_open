#pragma once

#include <QElapsedTimer>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/mediaserver_plugins/utils/plugin_manifest_base.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

/** Description of the vca analytics event. */
struct EventType: nx::vms::api::analytics::EventType
{
    // VCA-camera event type name (this name is sent by VCA-camera tcp notification server).
    QString internalName;
};
#define VcaEventType_Fields EventType_Fields(internalName)

struct PluginManifest: nx::mediaserver_plugins::utils::PluginManifestBase
{
    QList<EventType> outputEventTypes;
};
#define VcaPluginManifest_Fields PluginManifestBase_Fields (outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(PluginManifest, (json))

bool operator==(const EventType& lhs, const EventType& rhs);

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
