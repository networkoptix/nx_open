#include "analytics_helper.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

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

    QSet<nx::api::AnalyticsEventTypeId> addedEvents;

    for (const auto& server: resourcePool()->getAllServers(Qn::AnyStatus))
    {
        for (const auto& manifest: server->analyticsDrivers())
        {
            for (const auto& eventType: manifest.outputEventTypes)
            {
                nx::api::AnalyticsEventTypeId id(manifest.driverId, eventType.eventTypeId);
                if (addedEvents.contains(id))
                    continue;
                addedEvents.insert(id);

                nx::api::AnalyticsEventTypeWithRef ref;
                ref.driverId = manifest.driverId;
                ref.driverName = manifest.driverName;
                ref.eventTypeId = eventType.eventTypeId;
                ref.eventName = eventType.eventName;
                result.push_back(ref);
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
