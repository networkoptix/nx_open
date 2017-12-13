#pragma once

#if defined(ENABLE_HANWHA)

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

struct Hanwha
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

    enum class EventItemType
    {
        // TODO: #dmishin fill proper event item types
        none,
        ein,
        zwei,
        drei
    };

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
        const Hanwha::EventDescriptor& eventDescriptorById(const QnUuid& id) const;
    private:
        mutable QMap<QString, QnUuid> m_idByInternalName;
        mutable QMap<QnUuid, EventDescriptor> m_recordById;

    };
    #define DriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)

};
Q_DECLARE_OPERATORS_FOR_FLAGS(Hanwha::EventTypeFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Hanwha::EventItemType)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Hanwha::EventTypeFlag)

struct HanwhaEvent
{
    nxpl::NX_GUID typeId;
    QString caption;
    QString description;
    boost::optional<int> channel;
    boost::optional<int> region;
    bool isActive = false;
    Hanwha::EventItemType itemType; //< e.g Gunshot for sound classification
    QString fullEventName;
};

using HanwhaEventList = std::vector<HanwhaEvent>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hanwha::EventDescriptor)
    (Hanwha::DriverManifest),
    (json)
)

} // namespace plugins
} // namespace mediaserver
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::mediaserver::plugins::Hanwha::EventTypeFlag)
    (nx::mediaserver::plugins::Hanwha::EventTypeFlags)
    (nx::mediaserver::plugins::Hanwha::EventItemType),
    (metatype)(numeric)(lexical)
)

#endif // defined(ENABLE_HANWHA)
