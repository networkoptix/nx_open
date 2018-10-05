#include "metadata_handler.h"

#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>

#include <plugins/plugin_tools.h>
#include <nx/mediaserver_plugins/utils/uuid.h>

#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <core/resource/security_cam_resource.h>
#include <nx/mediaserver/event/event_connector.h>
#include <analytics/common/object_detection_metadata.h>
#include <nx/fusion/model_functions.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <media_server/media_server_module.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

MetadataHandler::MetadataHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
    connect(this, &MetadataHandler::sdkEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_analyticsSdkEvent,
        Qt::QueuedConnection);
}

nx::vms::api::analytics::EventType MetadataHandler::eventTypeDescriptor(const QString& eventTypeId) const
{
    for (const auto& descriptor: m_manifest.outputEventTypes)
    {
        if (descriptor.id == eventTypeId)
            return descriptor;
    }
    return nx::vms::api::analytics::EventType();
}

void MetadataHandler::handleMetadata(
    Error error,
    MetadataPacket* metadata)
{
    if (metadata == nullptr)
    {
        NX_VERBOSE(this) << "WARNING: Received null metadata packet; ignoring";
        return;
    }

    if (error != Error::noError)
    {
        NX_VERBOSE(this) << "WARNING: Received metadata packet along with an error ccde "
            << (int) error << "; ignoring";
        return;
    }

    bool handled = false;

    nxpt::ScopedRef<EventsMetadataPacket> eventsPacket(
        metadata->queryInterface(IID_EventsMetadataPacket));
    if (eventsPacket)
    {
        handleEventsPacket(std::move(eventsPacket));
        handled = true;
    }

    nxpt::ScopedRef<ObjectsMetadataPacket> objectsPacket(
        metadata->queryInterface(IID_ObjectsMetadataPacket));
    if (objectsPacket)
    {
        handleObjectsPacket(std::move(objectsPacket));
        handled = true;
    }

    if (!handled)
    {
        NX_VERBOSE(this) << "WARNING: Received unsupported metadata packet with timestampUsec "
            << metadata->timestampUsec() << ", durationUsec " << metadata->durationUsec()
            << "; ignoring";
    }
}

void MetadataHandler::handleEventsPacket(nxpt::ScopedRef<EventsMetadataPacket> packet)
{
    int eventsCount = 0;
    while (true)
    {
        nxpt::ScopedRef<MetadataItem> item(packet->nextItem(), /*increaseRef*/ false);
        if (!item)
            break;

        ++eventsCount;

        nxpt::ScopedRef<Event> eventData(item->queryInterface(IID_Event));
        if (eventData)
        {
            const int64_t timestampUsec = packet->timestampUsec();
            handleMetadataEvent(std::move(eventData), timestampUsec);
        }
        else
        {
            NX_VERBOSE(this) << __func__ << "(): ERROR: Received event does not implement Event";
        }
    }

    if (eventsCount == 0)
        NX_VERBOSE(this) << __func__ << "(): WARNING: Received empty event packet; ignoring";
}

void MetadataHandler::handleObjectsPacket(nxpt::ScopedRef<ObjectsMetadataPacket> packet)
{
    using namespace nx::mediaserver_plugins::utils;
    nx::common::metadata::DetectionMetadataPacket data;
    while (true)
    {
        nxpt::ScopedRef<Object> item(packet->nextItem(), /*increaseRef*/ false);
        if (!item)
            break;
        nx::common::metadata::DetectedObject object;
        object.objectTypeId = item->typeId();
        object.objectId = fromPluginGuidToQnUuid(item->id());
        const auto box = item->boundingBox();
        object.boundingBox = QRectF(box.x, box.y, box.width, box.height);

        NX_VERBOSE(this) << __func__ << lm("(): x %1, y %2, width %3, height %4, typeId %5, id %6")
            .args(box.x, box.y, box.width, box.height, object.objectTypeId, object.objectId);

        for (int i = 0; i < item->attributeCount(); ++i)
        {
            nx::common::metadata::Attribute attribute;
            attribute.name = QString::fromStdString(item->attribute(i)->name());
            attribute.value = QString::fromStdString(item->attribute(i)->value());
            object.labels.push_back(attribute);

            NX_VERBOSE(this) << __func__ << "(): Attribute:" << attribute.name << attribute.value;
        }
        data.objects.push_back(std::move(object));
    }
    if (data.objects.empty())
        NX_VERBOSE(this) << __func__ << "(): WARNING: ObjectsMetadataPacket is empty";
    data.timestampUsec = packet->timestampUsec();
    data.durationUsec = packet->durationUsec();
    data.deviceId = m_resource->getId();

    if (data.timestampUsec <= 0)
        NX_WARNING(this, "Invalid ObjectsMetadataPacket timestamp: %1", data.timestampUsec);

    if (m_dataReceptor && data.timestampUsec >= 0) //< Warn about 0 but still accept it.
        m_dataReceptor->putData(nx::common::metadata::toMetadataPacket(data));

    if (m_visualDebugger)
        m_visualDebugger->push(nx::common::metadata::toMetadataPacket(data));
}

void MetadataHandler::handleMetadataEvent(
    nxpt::ScopedRef<Event> eventData,
    qint64 timestampUsec)
{
    auto eventState = nx::vms::api::EventState::undefined;

    const auto eventTypeId = eventData->typeId();
    NX_VERBOSE(this) << __func__ << lm("(): typeId %1").args(eventTypeId);

    auto descriptor = eventTypeDescriptor(eventTypeId);
    if (descriptor.flags.testFlag(nx::vms::api::analytics::EventTypeFlag::stateDependent))
    {
        eventState = eventData->isActive()
            ? nx::vms::api::EventState::active
            : nx::vms::api::EventState::inactive;

        const bool isDuplicate = eventState == nx::vms::api::EventState::inactive
            && lastEventState(eventTypeId) == nx::vms::api::EventState::inactive;

        if (isDuplicate)
        {
            NX_VERBOSE(this) << __func__ << "(): Ignoring duplicate event";
            return;
        }
    }

    setLastEventState(eventTypeId, eventState);

    auto sdkEvent = nx::vms::event::AnalyticsSdkEventPtr::create(
        m_resource,
        m_manifest.pluginId,
        eventTypeId,
        eventState,
        eventData->caption(),
        eventData->description(),
        eventData->auxilaryData(),
        timestampUsec);

    if (m_resource->captureEvent(sdkEvent))
    {
        NX_VERBOSE(this) << __func__ << lm("(): Capturing event");
        return;
    }

    emit sdkEventTriggered(sdkEvent);
}

void MetadataHandler::setResource(const QnSecurityCamResourcePtr& resource)
{
    m_resource = resource;
}

void MetadataHandler::setManifest(const nx::vms::api::analytics::PluginManifest& manifest)
{
    m_manifest = manifest;
}

void MetadataHandler::registerDataReceptor(QnAbstractDataReceptor* dataReceptor)
{
    m_dataReceptor = dataReceptor;
}

void MetadataHandler::removeDataReceptor(QnAbstractDataReceptor* dataReceptor)
{
    m_dataReceptor = nullptr;
}

void MetadataHandler::setVisualDebugger(
    nx::debugging::AbstractVisualMetadataDebugger* visualDebugger)
{
    m_visualDebugger = visualDebugger;
}

nx::vms::api::EventState MetadataHandler::lastEventState(const QString& eventTypeId) const
{
    if (m_eventStateMap.contains(eventTypeId))
        return m_eventStateMap[eventTypeId];

    return nx::vms::api::EventState::inactive;
}

void MetadataHandler::setLastEventState(
    const QString& eventTypeId, nx::vms::api::EventState eventState)
{
    m_eventStateMap[eventTypeId] = eventState;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
