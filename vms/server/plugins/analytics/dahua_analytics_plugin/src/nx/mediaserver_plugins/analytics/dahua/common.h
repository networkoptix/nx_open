#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>
#include <QtCore/QDateTime>

#include <nx/utils/std/optional.h>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/mediaserver_plugins/utils/analytics/engine_manifest_base.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/sdk/analytics/events_metadata_packet.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {

struct Dahua
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
    #define DahuaEventType_Fields EventType_Fields (internalName) \
        (internalMonitoringName) \
        (description) \
        (positiveState) \
        (negativeState) \
        (regionDescription) \
        (dependedEvent)

    struct EngineManifest: nx::mediaserver_plugins::utils::analytics::EngineManifestBase
    {
        QList<EventType> eventTypes;

        QString eventTypeByInternalName(const QString& internalEventName) const;
        const Dahua::EventType& eventTypeDescriptorById(const QString& id) const;
        Dahua::EventType eventTypeDescriptorByInternalName(
            const QString& internalName) const;

    private:
        static QnMutex m_cachedIdMutex;
        static QMap<QString, QString> m_eventTypeIdByInternalName;
        static QMap<QString, EventType> m_eventTypeDescriptorById;

    };
    #define DahuaEngineManifest_Fields EngineManifestBase_Fields (eventTypes)
};

struct DahuaEvent
{
    QString typeId;
    QString caption;
    QString description;
    QDateTime dateTime;
    std::optional<int> channel;
    std::optional<int> region;
    bool isActive = false;
    QString picName;
};

using DahuaEventList = std::vector<DahuaEvent>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Dahua::EventType)
    (Dahua::EngineManifest),
    (json)
)

} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
