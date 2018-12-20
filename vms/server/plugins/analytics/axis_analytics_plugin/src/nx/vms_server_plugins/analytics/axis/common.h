#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>
#include <QElapsedTimer>

#include <boost/optional/optional.hpp>

#include <nx/fusion/model_functions_fwd.h>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>

#include "nx/axis/camera_controller.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace axis {

struct EventType: nx::vms::api::analytics::EventType
{
    QString topic;
    QString caption;
    QString eventTypeIdExternal;

    EventType() = default; //< Fusion needs default constructor.
    EventType(const nx::axis::SupportedEventType& supportedEventType);
    QString fullName() const { return topic + QString("/") + caption; }
};
#define AxisEventType_Fields EventType_Fields(topic)(name)

/**
  * The algorithm of building of the valid event type list:
  * The final list is a union of two lists. The 1st is eventTypes white list.
  * The 2nd is based on camera supported events. It is constructed in 3 steps:
  *  a) plugin asks the camera about all supported events;
  *  b) they are filtered with allowedTopics filter (only events with the allowed topics remain);
  *  c) event with descriptions form forbiddenDescriptions list are excised.
  */
struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
{
    QStringList allowedTopics; //< Prorietary. Topic filter.
    QStringList forbiddenDescriptions; //< Proprietary. Black list.
    QList<EventType> eventTypes; //< Extends the inherited field. White list.
};
#define AxisEngineManifest_Fields EngineManifestBase_Fields \
    (allowedTopics)(forbiddenDescriptions)(eventTypes)

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json))

} // namespace axis
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
