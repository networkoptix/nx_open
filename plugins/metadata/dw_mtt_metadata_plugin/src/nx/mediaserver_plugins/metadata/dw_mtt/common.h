#pragma once

#include <QElapsedTimer>

#include <nx/api/analytics/analytics_event.h>
#include <nx/api/analytics/driver_manifest.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

/** Description of the DwMtt analytics event. */
struct AnalyticsEventType: nx::api::Analytics::EventType
{
    // DWMTT-camera event type name (this name is sent by DWMTT-camera tcp notification server).
    QString internalName;
    QString alarmName;
    int group = 0;
};
#define DwMttAnalyticsEventType_Fields AnalyticsEventType_Fields(internalName)(alarmName)(group)

struct AnalyticsDriverManifest: nx::api::AnalyticsDriverManifestBase
{
    QList<QString> supportedCameraModels; //< Camera models supported by this plugin.
    QList<AnalyticsEventType> outputEventTypes;
    bool supportsModel(const QString& model) const noexcept;
};
#define DwMttAnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields (supportedCameraModels)(outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

bool operator==(const AnalyticsEventType& lh, const AnalyticsEventType& rh);

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
