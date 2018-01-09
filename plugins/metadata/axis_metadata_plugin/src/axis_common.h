#pragma once

#include "camera_controller.h"

#include <QtCore/QString>
#include <QtCore/QFlag>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver {
namespace plugins {

struct Axis
{
    Q_GADGET

public:

    //########## THROW AWAY
    struct EventDescriptor: public nx::api::AnalyticsEventType
    {
        QString internalName;
        QString internalMonitoringName;
        QString description;
        QString positiveState;
        QString negativeState;
        QString regionDescription;
    };
    #define EventDescriptor_Fields AnalyticsEventType_Fields (internalName)\
        (internalMonitoringName)\
        (description)\
        (positiveState)\
        (negativeState)\
        (regionDescription)

    struct DriverManifest: public nx::api::AnalyticsDriverManifestBase
    {
        QList<EventDescriptor> outputEventTypes;

    private:
        mutable QMap<QString, QnUuid> m_idByInternalName;

    };
    #define DriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)
};

struct AxisEvent
{
    nxpl::NX_GUID typeId;
    QString caption;
    QString description;
    bool isActive = false;
    QString fullEventName;
    bool isStatefull;
};

using AxisEventList = std::vector<AxisEvent>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
//    (Axis::EventDescriptor)
    (Axis::DriverManifest),
    (json)
)

QString serializeEvent(const AxisEvent& event);
QString serializeEvents(const QList<AxisEvent>& events);

AxisEvent convertEvent(const nx::axis::SupportedEvent& supportedEvent);

} // namespace plugins
} // namespace mediaserver
} // namespace nx
