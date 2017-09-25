#include "analytics_helper.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

namespace {

/**
* Description of the single analytics event type.
*/
struct AnalyticsEventTypeId
{
    QnUuid driverId;
    QnUuid eventTypeId;

    bool operator==(const AnalyticsEventTypeId& other) const
    {
        return driverId == other.driverId && eventTypeId == other.eventTypeId;
    }

    friend uint qHash(const AnalyticsEventTypeId& key)
    {
        return qHash(key.driverId) ^ qHash(key.eventTypeId);
    }
};

/** Struct for keeping unique list of analytics event types. */
class AnalyticsEventTypeWithRefStorage
{
    using EventDescriptor = nx::vms::event::AnalyticsHelper::EventDescriptor;

public:
    AnalyticsEventTypeWithRefStorage(QList<EventDescriptor>* data):
        data(data)
    {}

    void addUnique(const nx::api::AnalyticsDriverManifest& manifest,
        const nx::api::AnalyticsEventType& eventType)
    {
        AnalyticsEventTypeId id{manifest.driverId, eventType.eventTypeId};
        if (keys.contains(id))
            return;

        keys.insert(id);
        EventDescriptor ref;
        ref.driverId = manifest.driverId;
        ref.driverName = manifest.driverName;
        ref.eventTypeId = eventType.eventTypeId;
        ref.eventName = eventType.eventName;
        data->push_back(ref);
    }

private:
    QSet<AnalyticsEventTypeId> keys;
    QList<EventDescriptor>* data;
};

} // namespace

namespace nx {
namespace vms {
namespace event {

AnalyticsHelper::AnalyticsHelper(QnCommonModule* commonModule, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(commonModule)
{
}

QList<AnalyticsHelper::EventDescriptor> AnalyticsHelper::systemSupportedAnaliticEvents() const
{
    QList<EventDescriptor> result;
    AnalyticsEventTypeWithRefStorage storage(&result);

    for (const auto& server: resourcePool()->getAllServers(Qn::AnyStatus))
    {
        for (const auto& manifest: server->analyticsDrivers())
        {
            for (const auto& eventType: manifest.outputEventTypes)
                storage.addUnique(manifest, eventType);
        }
    }
    return result;
}

QList<AnalyticsHelper::EventDescriptor> AnalyticsHelper::supportedAnalyticsEvents(
    const QnVirtualCameraResourceList& cameras)
{
    QList<EventDescriptor> result;
    AnalyticsEventTypeWithRefStorage storage(&result);

    for (const auto& camera: cameras)
    {
        const auto server = camera->getParentServer();
        NX_ASSERT(server);
        if (!server)
            continue;

        QSet<QnUuid> allowedEvents = camera->analyticsSupportedEvents().toSet();

        for (const auto& manifest: server->analyticsDrivers())
        {
            for (const auto& eventType: manifest.outputEventTypes)
            {
                if (!allowedEvents.contains(eventType.eventTypeId))
                    continue;

                storage.addUnique(manifest, eventType);
            }
        }
    }
    return result;
}

bool AnalyticsHelper::hasDifferentDrivers(const QList<EventDescriptor>& events)
{
    if (events.empty())
        return false;

    const auto firstDriverId = events[0].driverId;
    return std::any_of(events.cbegin() + 1, events.cend(),
        [&firstDriverId](const EventDescriptor& eventType)
        {
            return eventType.driverId != firstDriverId;
        });
}

QString AnalyticsHelper::eventName(const QnVirtualCameraResourcePtr& camera,
    const QnUuid& eventTypeId,
    const QString& locale)
{
    NX_ASSERT(camera);
    if (!camera)
        return QString();

    const auto server = camera->getParentServer();
    NX_ASSERT(server);
    if (!server)
        return QString();

    for (const auto& manifest: server->analyticsDrivers())
    {
        for (const auto &eventType: manifest.outputEventTypes)
        {
            if (eventType.eventTypeId == eventTypeId)
                return eventType.eventName.text(locale);
        };
    }

    return QString();
}


} // namespace event
} // namespace vms
} // namespace nx
