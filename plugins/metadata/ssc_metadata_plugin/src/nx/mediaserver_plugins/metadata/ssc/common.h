#pragma once

#include <QElapsedTimer>

#include <nx/api/analytics/analytics_event.h>
#include <nx/api/analytics/driver_manifest.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace ssc {

/** Description of the SSC analytics event. */
struct AnalyticsEventType: nx::api::Analytics::EventType
{
    // SSC-camera event type name: "camera" or "reset".
    QString internalName;
};
#define SscAnalyticsEventType_Fields AnalyticsEventType_Fields(internalName)

struct AnalyticsDriverManifest: nx::api::AnalyticsDriverManifestBase
{
    QString serialPortName;
    QList<QString> supportedCameraModels; //< Camera models supported by this plugin.
    QList<AnalyticsEventType> outputEventTypes;
    bool supportsModel(const QString& model) const noexcept;
};
#define SscAnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields (serialPortName)(supportedCameraModels)(outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

bool operator==(const AnalyticsEventType& lh, const AnalyticsEventType& rh);

} // namespace ssc
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
