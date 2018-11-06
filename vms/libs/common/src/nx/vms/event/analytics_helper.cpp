#include "analytics_helper.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

namespace {

template<typename TypeStruct>
QString getTypeName(
    const QnVirtualCameraResourcePtr& camera,
    const QString& typeId,
    const QString& locale,
    QList<TypeStruct> nx::vms::api::analytics::EngineManifest::* list)
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
        for (const auto& item: manifest.*list)
        {
            if (item.id == typeId)
                return item.name.text(locale);
        };
    }

    return QString();
}

/**
 * Description of the single analytics event type.
 */
struct AnalyticsEventTypeId
{
    QString pluginId;
    QString eventTypeId;

    bool operator==(const AnalyticsEventTypeId& other) const
    {
        return pluginId == other.pluginId && eventTypeId == other.eventTypeId;
    }

    friend uint qHash(const AnalyticsEventTypeId& key)
    {
        return qHash(key.pluginId) ^ qHash(key.eventTypeId);
    }
};

/** Struct for keeping unique list of analytics event types. */
class AnalyticsEventTypeWithRefStorage
{
    using EventTypeDescriptor = nx::vms::event::AnalyticsHelper::EventTypeDescriptor;

public:
    AnalyticsEventTypeWithRefStorage(QList<EventTypeDescriptor>* data): data(data) {}

    void addUnique(const nx::vms::api::analytics::EngineManifest& engineManifest,
        const nx::vms::api::analytics::EventType& eventType)
    {
        AnalyticsEventTypeId id{engineManifest.pluginId, eventType.id};
        if (keys.contains(id))
            return;

        keys.insert(id);
        EventTypeDescriptor descriptor(eventType);
        descriptor.pluginId = engineManifest.pluginId;
        descriptor.pluginName = engineManifest.pluginName;
        data->push_back(descriptor);
    }

private:
    QSet<AnalyticsEventTypeId> keys;
    QList<EventTypeDescriptor>* data;
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

AnalyticsHelper::EventTypeDescriptor AnalyticsHelper::eventTypeDescriptor(
    const QString& eventTypeId) const
{
    static QnMutex mutex;
    static QMap<QString, AnalyticsHelper::EventTypeDescriptor> cachedData;

    QnMutexLocker lock(&mutex);
    const auto itr = cachedData.find(eventTypeId);
    if (itr != cachedData.end())
        return *itr;

    for (const auto& server: resourcePool()->getAllServers(Qn::AnyStatus))
    {
        for (const auto& manifest: server->analyticsDrivers())
        {
            for (const auto& eventType: manifest.eventTypes)
            {
                if (eventType.id == eventTypeId)
                {
                    cachedData[eventTypeId] = eventType;
                    return eventType;
                }
            }
        }
    }
    return AnalyticsHelper::EventTypeDescriptor();
}

nx::vms::api::analytics::Group AnalyticsHelper::groupDescriptor(const QString& groupId) const
{
    static QnMutex mutex;
    static QMap<QString, nx::vms::api::analytics::Group> cachedData;

    QnMutexLocker lock(&mutex);
    const auto itr = cachedData.find(groupId);
    if (itr != cachedData.end())
        return *itr;

    for (const auto& server: resourcePool()->getAllServers(Qn::AnyStatus))
    {
        for (const auto& manifest: server->analyticsDrivers())
        {
            for (const auto& group: manifest.groups)
            {
                if (group.id == groupId)
                {
                    cachedData[groupId] = group;
                    return group;
                }
            }
        }
    }
    return nx::vms::api::analytics::Group();
}

QList<AnalyticsHelper::EventTypeDescriptor> AnalyticsHelper::systemSupportedAnalyticsEvents() const
{
    QList<EventTypeDescriptor> result;
    AnalyticsEventTypeWithRefStorage storage(&result);

    for (const auto& server: resourcePool()->getAllServers(Qn::AnyStatus))
    {
        for (const auto& manifest: server->analyticsDrivers())
        {
            for (const auto& eventType: manifest.eventTypes)
                storage.addUnique(manifest, eventType);
        }
    }
    return result;
}

QList<AnalyticsHelper::EventTypeDescriptor> AnalyticsHelper::systemCameraIndependentAnalyticsEvents()
    const
{
    return cameraIndependentAnalyticsEvents(resourcePool()->getAllServers(Qn::AnyStatus));
}

QList<AnalyticsHelper::EventTypeDescriptor> AnalyticsHelper::supportedAnalyticsEvents(
    const QnVirtualCameraResourceList& cameras)
{
    QList<EventTypeDescriptor> result;
    AnalyticsEventTypeWithRefStorage storage(&result);

    for (const auto& camera: cameras)
    {
        const auto server = camera->getParentServer();
        NX_ASSERT(server);
        if (!server)
            continue;

        QSet<QString> supportedEventTypeIds = camera->supportedAnalyticsEventTypeIds().toSet();

        for (const auto& manifest: server->analyticsDrivers())
        {
            for (const auto& eventType: manifest.eventTypes)
            {
                if (!supportedEventTypeIds.contains(eventType.id))
                    continue;

                storage.addUnique(manifest, eventType);
            }
        }
    }
    return result;
}

QList<AnalyticsHelper::EventTypeDescriptor> AnalyticsHelper::cameraIndependentAnalyticsEvents(
    const QnMediaServerResourceList& servers)
{
    QList<EventTypeDescriptor> result;
    AnalyticsEventTypeWithRefStorage storage(&result);

    for (const auto& server: servers)
    {
        for (const auto& manifest: server->analyticsDrivers())
        {
            if (manifest.capabilities.testFlag(
                nx::vms::api::analytics::EngineManifest::Capability::cameraModelIndependent))
            {
                for (const auto& eventType: manifest.eventTypes)
                    storage.addUnique(manifest, eventType);
            }
        }
    }

    return result;
}

bool AnalyticsHelper::hasDifferentDrivers(const QList<EventTypeDescriptor>& events)
{
    if (events.empty())
        return false;

    const auto firstDriverId = events[0].pluginId;
    return std::any_of(events.cbegin() + 1, events.cend(),
        [&firstDriverId](const EventTypeDescriptor& eventType)
        {
            return eventType.pluginId != firstDriverId;
        });
}

QString AnalyticsHelper::eventTypeName(const QnVirtualCameraResourcePtr& camera,
    const QString& eventTypeId,
    const QString& locale)
{
    return getTypeName(
        camera, eventTypeId, locale, &nx::vms::api::analytics::EngineManifest::eventTypes);
}

QString AnalyticsHelper::objectTypeName(const QnVirtualCameraResourcePtr& camera,
    const QString& objectTypeId,
    const QString& locale)
{
    return getTypeName(
        camera, objectTypeId, locale, &nx::vms::api::analytics::EngineManifest::objectTypes);
}

QList<AnalyticsHelper::PluginActions> AnalyticsHelper::availableActions(
    const QnMediaServerResourceList& servers, const QString& objectTypeId)
{
    QList<AnalyticsHelper::PluginActions> actions;
    if (servers.isEmpty())
        return actions;

    for (const auto& server: servers.toSet())
    {
        const auto& manifests = server->analyticsDrivers();
        for (const auto& manifest: manifests)
        {
            PluginActions actionsOfPlugin;
            actionsOfPlugin.pluginId = manifest.pluginId;

            for (const auto& action: manifest.objectActions)
            {
                if (action.supportedObjectTypeIds.contains(objectTypeId))
                    actionsOfPlugin.actions.append(action);
            }

            if (!actionsOfPlugin.actions.isEmpty())
                actions.append(actionsOfPlugin);
        }
    }

    return actions;
}

} // namespace event
} // namespace vms
} // namespace nx
