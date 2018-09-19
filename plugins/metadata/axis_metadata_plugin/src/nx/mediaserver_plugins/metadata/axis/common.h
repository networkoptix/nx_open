#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>
#include <QElapsedTimer>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/fusion/model_functions_fwd.h>

#include "nx/axis/camera_controller.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

struct AnalyticsEventType: nx::api::Analytics::EventType
{
    QString topic;
    QString name;
    nxpl::NX_GUID eventTypeIdExternal;

    AnalyticsEventType() = default; //< Fusion needs default constructor.
    AnalyticsEventType(const nx::axis::SupportedEvent& supportedEvent);
    QString fullName() const { return topic + QString("/") + name; }
};
#define AxisAnalyticsEventType_Fields AnalyticsEventType_Fields(topic)(name)

/**
  * The algorithm of building of the valid event type list:
  * The final list is a union of two lists. The 1st is outputEventTypes white list.
  * The 2nd is based on camera supported events. It is constructed in 3 steps:
  *  a) plugin asks the camera about all supported events;
  *  b) they are filtered with allowedTopics filter (only events with the allowed topics remain);
  *  c) event with descriptions form forbiddenDescriptions list are excised.
  */
struct AnalyticsDriverManifest: nx::api::AnalyticsDriverManifestBase
{
    QStringList allowedTopics; //< topic filter
    QStringList forbiddenDescriptions; //< black list
    QList<AnalyticsEventType> outputEventTypes; //< white list
};
#define AxisAnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields(allowedTopics)(forbiddenDescriptions)(outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
