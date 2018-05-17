#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>
#include <QtCore/QDateTime>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/sdk/metadata/events_metadata_packet.h>

namespace nx {
namespace mediaserver {
namespace plugins {

struct Hikvision
{
public:
    struct EventDescriptor: public nx::api::Analytics::EventType
    {
        QString internalName;
        QString internalMonitoringName;
        QString description;
        QString positiveState;
        QString negativeState;
        QString regionDescription;
        QString dependedEvent;
    };
    #define EventDescriptor_Fields AnalyticsEventType_Fields (internalName)\
        (internalMonitoringName)\
        (description)\
        (positiveState)\
        (negativeState)\
        (regionDescription)\
        (dependedEvent)

    struct DriverManifest: public nx::api::AnalyticsDriverManifestBase
    {
        QList<EventDescriptor> outputEventTypes;

        QnUuid eventTypeByInternalName(const QString& internalEventName) const;
        const Hikvision::EventDescriptor& eventDescriptorById(const QnUuid& id) const;
        const Hikvision::EventDescriptor eventDescriptorByInternalName(const QString& internalName) const;
    private:
        static QnMutex m_cachedIdMutex;
        static QMap<QString, QnUuid> m_idByInternalName;
        static QMap<QnUuid, EventDescriptor> m_recordById;

    };
    #define DriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)

};
struct HikvisionEvent
{
    QDateTime dateTime;
    QnUuid typeId;
    QString caption;
    QString description;
    boost::optional<int> channel;
    boost::optional<int> region;
    bool isActive = false;
    QString picName;
};

using HikvisionEventList = std::vector<HikvisionEvent>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hikvision::EventDescriptor)
    (Hikvision::DriverManifest),
    (json)
)

} // namespace plugins
} // namespace mediaserver
} // namespace nx
