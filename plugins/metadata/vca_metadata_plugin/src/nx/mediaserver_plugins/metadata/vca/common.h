#pragma once

#include <QElapsedTimer>

#include <nx/api/analytics/analytics_event.h>
#include <nx/api/analytics/driver_manifest.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca
{

/**
 * Description of the vca analytics event.
 */
struct AnalyticsEventType: nx::api::Analytics::EventType
{
    // VCA-camera event type name (this name is sent by VCA-camera tcp notification server).
    QString internalName;
};
#define VcaAnalyticsEventType_Fields AnalyticsEventType_Fields(internalName)

struct AnalyticsDriverManifest: nx::api::AnalyticsDriverManifestBase
{
    QList<AnalyticsEventType> outputEventTypes;
};
#define VcaAnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

bool operator==(const AnalyticsEventType& lh, const AnalyticsEventType& rh);

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
