#pragma once

#if defined(ENABLED_HANWHA)

#include <QtCore/QString>
#include <QtCore/QFlag>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

struct Hanwha //< Used as a namespace; struct is required for Qt serialization of inner types.
{
    Q_GADGET
    Q_ENUMS(EventTypeFlag EventItemType)
    Q_FLAGS(EventTypeFlags)

public:
    enum EventTypeFlag
    {
        stateDependent = 1 << 0,
        regionDependent = 1 << 1,
        itemDependent = 1 << 2,
    };
    Q_DECLARE_FLAGS(EventTypeFlags, EventTypeFlag)

    enum class EventItemType
    {
        // TODO: #dmishin: Populate with proper event item types.
        none,
        t1,
        t2,
        t3,
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
    #define EventDescriptor_Fields AnalyticsEventType_Fields \
        (internalName) \
        (internalMonitoringName) \
        (description) \
        (positiveState) \
        (negativeState) \
        (flags) \
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

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hanwha::EventDescriptor)
    (Hanwha::DriverManifest),
    (json)
)

struct Event
{
    nxpl::NX_GUID typeId;
    QString caption;
    QString description;
    boost::optional<int> channel;
    boost::optional<int> region;
    bool isActive = false;
    Hanwha::EventItemType itemType; //< e.g Gunshot for sound classification.
    QString fullEventName;
};

using EventList = std::vector<Event>;

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::mediaserver_plugins::metadata::hanwha::Hanwha::EventTypeFlag)
    (nx::mediaserver_plugins::metadata::hanwha::Hanwha::EventTypeFlags)
    (nx::mediaserver_plugins::metadata::hanwha::Hanwha::EventItemType),
    (metatype)(numeric)(lexical)
)

#endif // defined(ENABLED_HANWHA)
