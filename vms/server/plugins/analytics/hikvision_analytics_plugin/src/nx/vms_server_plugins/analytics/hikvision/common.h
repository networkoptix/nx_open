#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>

#include <boost/optional/optional.hpp>

#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/thread/mutex.h>

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

    struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
    {
        QList<EventType> eventTypes;

        QString eventTypeByInternalName(const QString& internalEventName) const;
        const Hikvision::EventType& eventTypeDescriptorById(const QString& id) const;
        Hikvision::EventType eventTypeDescriptorByInternalName(
            const QString& internalName) const;

    private:
        static QnMutex m_cachedIdMutex;
        static QMap<QString, QString> m_eventTypeIdByInternalName;
        static QMap<QString, EventType> m_eventTypeDescriptorById;

    };
    #define HikvisionEngineManifest_Fields EngineManifestBase_Fields (eventTypes)
};

struct HikvisionEvent
{
    QDateTime dateTime;
    QString typeId;
    QString caption;
    QString description;
    boost::optional<int> channel;
    boost::optional<int> region;
    bool isActive = false;
    QString picName;
};

using HikvisionEventList = std::vector<HikvisionEvent>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hikvision::EventType)
    (Hikvision::EngineManifest),
    (json)
)

} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
