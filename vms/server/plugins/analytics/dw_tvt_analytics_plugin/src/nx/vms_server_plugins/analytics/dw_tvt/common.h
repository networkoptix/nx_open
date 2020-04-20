#pragma once

#include <QElapsedTimer>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms_server_plugins::analytics::dw_tvt {

/** Description of the DwTvt analytics event. */
struct EventType: nx::vms::api::analytics::EventType
{
    // DWTVT-camera event type name (this name is sent by DWTVT-camera tcp notification server).
    QString internalName;
    QString alarmName;
    int group = 0;
};
#define DwTvtEventType_Fields EventType_Fields(internalName)(alarmName)(group)

struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
{
    QList<QString> supportedCameraModels; //< Proprietary. Camera models supported by this plugin.
    QList<EventType> eventTypes;

    bool supportsModel(const QString& model) const noexcept;
};
#define DwTvtEngineManifest_Fields EngineManifestBase_Fields \
    (supportedCameraModels)(eventTypes)

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json))

bool operator==(const EventType& lh, const EventType& rh);

} // nx::vms_server_plugins::analytics::dw_tvt
