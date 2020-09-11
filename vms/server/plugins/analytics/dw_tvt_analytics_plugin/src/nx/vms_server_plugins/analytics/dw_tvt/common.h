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
    bool restricted = false; //< Partially supported camera models ignore restricted event types.
    int group = 0;
};
#define DwTvtEventType_Fields EventType_Fields(internalName)(alarmName)(restricted)(group)

struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
{
    QList<QString> supportedCameraVendors; //< Proprietary. Camera vendors supported by this plugin.
    QList<QString> supportedCameraModels; //< Proprietary. Camera models supported by this plugin.
    QList<QString> partlySupportedCameraModels; //< Proprietary. Camera models supported by this plugin.
    QList<EventType> eventTypes;

    bool supportsModelCompletely(const QString& model) const noexcept;
    bool supportsModelPartly(const QString& model) const noexcept;
    bool supportsModel(const QString& model) const noexcept;
    QList<QString> eventTypeIdListForModel(const QString& model) const noexcept;
};
#define DwTvtEngineManifest_Fields EngineManifestBase_Fields \
    (supportedCameraVendors)(supportedCameraModels)(partlySupportedCameraModels)(eventTypes)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((EventType)(EngineManifest), (json))

} // nx::vms_server_plugins::analytics::dw_tvt
