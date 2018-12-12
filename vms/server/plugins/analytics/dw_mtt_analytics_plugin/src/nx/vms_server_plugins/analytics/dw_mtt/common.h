#pragma once

#include <QElapsedTimer>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
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

struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
{
    QList<QString> supportedCameraModels; //< Proprietary. Camera models supported by this plugin.
    QList<EventType> eventTypes;

    bool supportsModel(const QString& model) const noexcept;
};
#define DwMttEngineManifest_Fields EngineManifestBase_Fields \
    (supportedCameraModels)(eventTypes)

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json))

bool operator==(const EventType& lh, const EventType& rh);

} // namespace dw_mtt
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
