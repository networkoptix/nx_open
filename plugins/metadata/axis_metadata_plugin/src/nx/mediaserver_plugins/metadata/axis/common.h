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

    AnalyticsEventType() = default;
    AnalyticsEventType(const nx::axis::SupportedEvent& supportedEvent);
    QString fullName() const { return topic + QString("/") + name; }
    bool isStateful() const noexcept
    {
        return flags.testFlag(nx::api::Analytics::EventTypeFlag::stateDependent);
    }
};

#define AxisAnalyticsEventType_Fields AnalyticsEventType_Fields(topic)(name)

struct AnalyticsDriverManifest: nx::api::AnalyticsDriverManifestBase
{
    QList<AnalyticsEventType> outputEventTypes;
};
#define AxisAnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields(outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

struct AnalyticsEventTypeExtended: AnalyticsEventType
{
    mutable QElapsedTimer elapsedTimer;
};

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
