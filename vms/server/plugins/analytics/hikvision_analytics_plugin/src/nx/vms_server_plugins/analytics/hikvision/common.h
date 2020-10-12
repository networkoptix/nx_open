#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>

#include <optional>

#include <nx/sdk/uuid.h>
#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/thread/mutex.h>

#include "geometry.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {

struct Hikvision
{
public:
    struct EventType: public nx::vms::api::analytics::EventType
    {
        QString internalName;
        QString internalMonitoringName;
        QString description;
        QString positiveState;
        QString negativeState;
        QString regionDescription;
        QString dependedEvent;
    };
    #define HikvisionEventType_Fields EventType_Fields (internalName) \
        (internalMonitoringName) \
        (description) \
        (positiveState) \
        (negativeState) \
        (regionDescription) \
        (dependedEvent)

    struct ObjectType: public nx::vms::api::analytics::ObjectType
    {
        QSet<QString> sourceCapabilities;
    };
    #define HikvisionObjectType_Fields ObjectType_Fields \
        (sourceCapabilities)

    struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
    {
        QList<EventType> eventTypes;
        QList<ObjectType> objectTypes;

        QString eventTypeIdByInternalName(const QString& internalEventName) const;
        const Hikvision::EventType& eventTypeById(const QString& id) const;
        const Hikvision::EventType& eventTypeByInternalName(const QString& internalName) const;

    private:
        static QnMutex m_cachedIdMutex;
        static QMap<QString, QString> m_eventTypeIdByInternalName;
        static QMap<QString, EventType> m_eventTypeById;

    };
    #define HikvisionEngineManifest_Fields EngineManifestBase_Fields \
        (eventTypes) \
        (objectTypes)
};

struct HikvisionEvent
{
    QDateTime dateTime;
    QString typeId;
    nx::sdk::Uuid trackId;
    QString caption;
    QString description;
    std::optional<int> channel;
    std::optional<hikvision::Region> region;
    bool isActive = false;
    QString picName;
    bool isThermal() const { return typeId.contains("Thermal"); }
};

struct EventWithRegions
{
    HikvisionEvent event;
    hikvision::Regions regions;
};

using HikvisionEventList = std::vector<HikvisionEvent>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hikvision::EventType)
    (Hikvision::ObjectType)
    (Hikvision::EngineManifest),
    (json)
)

} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
