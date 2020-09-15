#pragma once

#include <optional>
#include <set>

#include <QtCore/QString>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json.h>

namespace nx::vms_server_plugins::analytics::bosch {

struct Bosch //< Used as a namespace; struct is required for Qt serialization of inner types.
{
public:
    struct EventType: public nx::vms::api::analytics::EventType
    {
        bool autodetect = false;
        QString internalName;
        QString description;
        QString positiveState;
        QString negativeState;
        QString regionDescription;

        QString fullDescription(bool isActive = true) const;

        const QString& key() const { return internalName; } //< The field to sort/search by.
        using KeyType = QString;
    };
    #define BoschEventType_Fields EventType_Fields \
        (autodetect) \
        (internalName) \
        (description) \
        (positiveState) \
        (negativeState) \
        (regionDescription)

    struct ObjectType: public nx::vms::api::analytics::ObjectType
    {
        QString internalName;

        const QString& key() const { return internalName; } //< The field to sort/search by.
        using KeyType = QString;
    };
    #define BoschObjectType_Fields ObjectType_Fields (internalName)

    template<class T>
    struct Comparator
    {
        using is_transparent = void;
        using K = typename T::KeyType;
        bool operator()(const T& lhs, const T& rhs) const { return lhs.key() < rhs.key(); }
        bool operator()(const T& lhs, const K& key) const { return lhs.key() < key; }
        bool operator()(const K& key, const T& rhs) const { return key < rhs.key(); }
    };

    struct EngineManifest: nx::vms::api::analytics::EngineManifest
    {
        std::set<EventType, Comparator<EventType>> eventTypes;
        std::set<ObjectType, Comparator<ObjectType>> objectTypes;

        const EventType* eventTypeByInternalName(const QString& eventName) const;
        const ObjectType* objectTypeByInternalName(const QString& objectName) const;
    };
    #define BoschEngineManifest_Fields EngineManifest_Fields

    struct DeviceAgentManifest: nx::vms::api::analytics::DeviceAgentManifest
    {
        // No fields added.
    };
    #define BoschDeviceAgentManifest_Fields DeviceAgentManifest_Fields

};
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Bosch::EventType)
    (Bosch::ObjectType)
    (Bosch::EngineManifest)
    (Bosch::DeviceAgentManifest),
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
    QString fullEventName;
};

using EventList = std::vector<Event>;

} // namespace nx::vms_server_plugins::analytics::bosch
