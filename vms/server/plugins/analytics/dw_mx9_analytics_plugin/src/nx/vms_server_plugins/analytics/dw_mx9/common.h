#pragma once

#include <QElapsedTimer>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms_server_plugins::analytics::dw_mx9 {

/** Description of the DwMx9 analytics event. */
struct EventType: nx::vms::api::analytics::EventType
{
    /**
     * DW Mx9 (aka TVT) camera event type name. This name is sent by such camera's tcp notification
     * server.
     */
    QString internalName;

    QString alarmName;

    /** NOTE: Partially supported camera models ignore restricted event types. */
    bool restricted = false;

    int group = 0;
};
#define DwMx9EventType_Fields EventType_Fields(internalName)(alarmName)(restricted)(group)

struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
{
    /** Proprietary. Camera vendors supported by this plugin. */
    QList<QString> supportedCameraVendors;

    /** Proprietary. Camera models supported by this plugin. */
    QList<QString> supportedCameraModels;

    /** Proprietary. Camera models supported by this plugin. */
    QList<QString> partlySupportedCameraModels;

    QList<EventType> eventTypes;

    bool supportsModelCompletely(const QString& model) const noexcept;
    bool supportsModelPartly(const QString& model) const noexcept;
    bool supportsModel(const QString& model) const noexcept;
    QList<QString> eventTypeIdListForModel(const QString& model) const noexcept;
};
#define DwMx9EngineManifest_Fields EngineManifestBase_Fields \
    (supportedCameraVendors)(supportedCameraModels)(partlySupportedCameraModels)(eventTypes)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((EventType)(EngineManifest), (json))

} // namespace nx::vms_server_plugins::analytics::dw_mx9
