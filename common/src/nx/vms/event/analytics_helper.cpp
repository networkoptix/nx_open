#include "analytics_helper.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

namespace {

/** Struct for keeping unique list of analytics event types. */
class AnalyticsEventTypeWithRefStorage
{
public:
    AnalyticsEventTypeWithRefStorage(QList<nx::api::AnalyticsEventTypeWithRef>* data):
        data(data)
    {}

    void addUnique(const nx::api::AnalyticsDriverManifest& manifest,
        const nx::api::AnalyticsEventType& eventType)
    {
        nx::api::AnalyticsEventTypeId id(manifest.driverId, eventType.eventTypeId);
        if (keys.contains(id))
            return;

        keys.insert(id);
        nx::api::AnalyticsEventTypeWithRef ref;
        ref.driverId = manifest.driverId;
        ref.driverName = manifest.driverName;
        ref.eventTypeId = eventType.eventTypeId;
        ref.eventName = eventType.eventName;
        data->push_back(ref);
    }

private:
    QSet<nx::api::AnalyticsEventTypeId> keys;
    QList<nx::api::AnalyticsEventTypeWithRef>* data;
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

QList<nx::api::AnalyticsEventTypeWithRef> AnalyticsHelper::analyticsEvents() const
{
    QList<nx::api::AnalyticsEventTypeWithRef> result;
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

QList<nx::api::AnalyticsEventTypeWithRef> AnalyticsHelper::analyticsEvents(
    const QnVirtualCameraResourceList& cameras)
{
    QList<nx::api::AnalyticsEventTypeWithRef> result;
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

bool AnalyticsHelper::hasDifferentDrivers(const QList<nx::api::AnalyticsEventTypeWithRef>& events)
{
    if (events.empty())
        return false;

    const auto firstDriverId = events[0].driverId;
    return std::any_of(events.cbegin() + 1, events.cend(),
        [&firstDriverId](const nx::api::AnalyticsEventTypeWithRef& eventType)
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
