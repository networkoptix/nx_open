#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>
#include <QtCore/QDateTime>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver {
namespace plugins {

struct Hikvision
{
    Q_GADGET
    Q_ENUMS(EventTypeFlag EventItemType)
    Q_FLAGS(EventTypeFlags)

public:
    enum EventTypeFlag
    {
        stateDependent = 0x1,
        regionDependent = 0x2,
        itemDependent = 0x4
    };
    Q_DECLARE_FLAGS(EventTypeFlags, EventTypeFlag)

    struct EventDescriptor: public nx::api::AnalyticsEventType
    {
        QString internalName;
        QString internalMonitoringName;
        QString description;
        QString positiveState;
        QString negativeState;
        EventTypeFlags flags;
        QString regionDescription;
    };
    #define EventDescriptor_Fields AnalyticsEventType_Fields (internalName)\
        (internalMonitoringName)\
        (description)\
        (positiveState)\
        (negativeState)\
        (flags)\
        (regionDescription)

    struct DriverManifest: public nx::api::AnalyticsDriverManifestBase
    {
        QList<EventDescriptor> outputEventTypes;

        QnUuid eventTypeByInternalName(const QString& internalEventName) const;
        const Hikvision::EventDescriptor& eventDescriptorById(const QnUuid& id) const;
    private:
        mutable QMap<QString, QnUuid> m_idByInternalName;
        mutable QMap<QnUuid, EventDescriptor> m_recordById;

    };
    #define DriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)

};
Q_DECLARE_OPERATORS_FOR_FLAGS(Hikvision::EventTypeFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Hikvision::EventTypeFlag)

struct HikvisionEvent
{
    QDateTime dateTime;
    QnUuid typeId;
    QString caption;
    QString description;
    boost::optional<int> channel;
    boost::optional<int> region;
    bool isActive = false;
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

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::mediaserver::plugins::Hikvision::EventTypeFlag)
    (nx::mediaserver::plugins::Hikvision::EventTypeFlags)
    (metatype)(numeric)(lexical)
)
