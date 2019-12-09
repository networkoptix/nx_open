#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>

#include <boost/optional/optional.hpp>

#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

struct Hanwha //< Used as a namespace; struct is required for Qt serialization of inner types.
{
    Q_GADGET
    Q_ENUMS(EventItemType)

public:

    enum class EventItemType
    {
        // TODO: #dmishin: Populate with proper event item types.
        none,
        t1,
        t2,
        t3,
    };

    struct EventType: public nx::vms::api::analytics::EventType
    {
        QString internalName;
        QString internalMonitoringName;
        QString description;
        QString positiveState;
        QString negativeState;
        QString regionDescription;
    };
    #define HanwhaEventType_Fields EventType_Fields \
        (internalName) \
        (internalMonitoringName) \
        (description) \
        (positiveState) \
        (negativeState) \
        (regionDescription)

    struct ObjectType: public nx::vms::api::analytics::ObjectType
    {
        // Empty class, reserved for future use.
    };
    #define HanwhaObjectType_Fields ObjectType_Fields

    struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
    {
        QList<EventType> eventTypes;
        QList<ObjectType> objectTypes;

        /** @return Null if not found. */
        QString eventTypeIdByName(const QString& eventName) const;

        const Hanwha::EventType& eventTypeDescriptorById(const QString& id) const;

    private:
        mutable QMap<QString, QString> m_eventTypeIdByInternalName;
        mutable QMap<QString, EventType> m_eventTypeDescriptorById;

    };
    #define HanwhaEngineManifest_Fields EngineManifestBase_Fields (eventTypes)(objectTypes)
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Hanwha::EventItemType)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hanwha::EventType)
    (Hanwha::ObjectType)
    (Hanwha::EngineManifest),
    (json)
)

struct Event
{
    QString typeId;
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
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::vms_server_plugins::analytics::hanwha::Hanwha::EventItemType),
    (metatype)(numeric)(lexical)
)
