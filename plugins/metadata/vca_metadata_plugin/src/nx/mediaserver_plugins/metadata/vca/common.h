#pragma once

#include <nx/api/analytics/analytics_event.h>
#include <nx/api/analytics/driver_manifest.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca //< This struct substitutes namespace because of fusion problems with namespaces.
{

/**
 * Description of the vca analytics event.
 */
struct VcaAnalyticsEventType: nx::api::Analytics::EventType
{
    // VCA-camera event type name (this name is sent by VCA-camera tcp notification server).
    QString internalName;
};
#define VcaAnalyticsEventType_Fields AnalyticsEventType_Fields(internalName)

struct VcaAnalyticsDriverManifest: nx::api::AnalyticsDriverManifestBase
{
    QList<VcaAnalyticsEventType> outputEventTypes;
};
#define VcaAnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(VcaAnalyticsEventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(VcaAnalyticsDriverManifest, (json))

bool operator==(const VcaAnalyticsEventType& lh, const VcaAnalyticsEventType& rh);

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
