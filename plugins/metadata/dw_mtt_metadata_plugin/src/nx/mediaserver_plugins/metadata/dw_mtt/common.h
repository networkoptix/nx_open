#pragma once

#include <QElapsedTimer>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/mediaserver_plugins/utils/plugin_manifest_base.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

/** Description of the DwMtt analytics event. */
struct EventType: nx::vms::api::analytics::EventType
{
    // DWMTT-camera event type name (this name is sent by DWMTT-camera tcp notification server).
    QString internalName;
    QString alarmName;
    int group = 0;
    bool unsupported = false;
};
#define DwMttEventType_Fields EventType_Fields(internalName)(alarmName)(group)(unsupported)

struct PluginManifest: nx::mediaserver_plugins::utils::PluginManifestBase
{
    QList<QString> supportedCameraModels; //< Proprietary. Camera models supported by this plugin.
    QList<EventType> outputEventTypes;
    
    bool supportsModel(const QString& model) const noexcept;
};
#define DwMttPluginManifest_Fields PluginManifestBase_Fields \
    (supportedCameraModels)(outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(PluginManifest, (json))

bool operator==(const EventType& lh, const EventType& rh);

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
