#pragma once

#include <optional>

#include <QtCore/QString>
#include <QtCore/QFlag>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json.h>

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
        QString internalName;
    };
    #define HanwhaObjectType_Fields ObjectType_Fields (internalName)

    struct EngineManifest: nx::vms::api::analytics::EngineManifest
    {
        QList<EventType> eventTypes;
        QList<ObjectType> objectTypes;

        QString eventTypeIdByName(const QString& eventName) const;
        const Hanwha::EventType& eventTypeDescriptorById(const QString& id) const;
        const Hanwha::ObjectType& objectTypeDescriptorById(const QString& id) const;

        void initializeObjectTypeMap();
        QString objectTypeIdByInternalName(const QString& eventName) const;

    private:
        mutable QMap<QString, QString> m_eventTypeIdByInternalName;
        mutable QMap<QString, EventType> m_eventTypeDescriptorById;
        mutable QMap<QString, ObjectType> m_objectTypeDescriptorById;

        QMap<QString, QString> m_objectTypeIdByInternalName;
    };
    #define HanwhaEngineManifest_Fields EngineManifest_Fields

    struct DeviceAgentManifest: nx::vms::api::analytics::DeviceAgentManifest
    {
    };
    #define HanwhaDeviceAgentManifest_Fields DeviceAgentManifest_Fields

    struct ObjectMetadataAttributeFilters
    {
        QList<QString> roots; //< XML element tag names from which to extract attributes
        QList<QString> discard; //< patterns of attribute names to discard

        struct RenameEntry
        {
            QString ifMatches; //< pattern to match attribute name to
            QString replaceWith; //< new attribute name
        };
        QList<RenameEntry> rename; //< attribute renaming map
    };
    #define HanwhaObjectMetadataAttributeFilters_Fields (roots)(discard)(rename)
    #define HanwhaObjectMetadataAttributeFiltersRenameEntry_Fields (ifMatches)(replaceWith)
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Hanwha::EventItemType)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hanwha::EventType)
    (Hanwha::ObjectType)
    (Hanwha::EngineManifest)
    (Hanwha::DeviceAgentManifest)
    (Hanwha::ObjectMetadataAttributeFilters)
    (Hanwha::ObjectMetadataAttributeFilters::RenameEntry),
    (json)
)

struct Event
{
    QString typeId;
    QString caption;
    QString description;
    std::optional<int> channel;
    std::optional<int> region;
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
