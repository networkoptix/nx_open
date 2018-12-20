#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>
#include <QtCore/QDateTime>

#include <nx/utils/std/optional.h>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/vms_server_plugins/utils/analytics/engine_manifest_base.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

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

struct EngineManifest: nx::vms_server_plugins::utils::analytics::EngineManifestBase
{
    QList<EventType> eventTypes;

    QString eventTypeByInternalName(const QString& internalEventName) const;
    const EventType& eventTypeDescriptorById(const QString& id) const;
    EventType eventTypeDescriptorByInternalName(
        const QString& internalName) const;

private:
    static QnMutex m_cachedIdMutex;
    static QMap<QString, QString> m_eventTypeIdByInternalName;
    static QMap<QString, EventType> m_eventTypeDescriptorById;

};
#define DahuaEngineManifest_Fields EngineManifestBase_Fields (eventTypes)

struct Event
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

using EventList = std::vector<Event>;

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json))

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
