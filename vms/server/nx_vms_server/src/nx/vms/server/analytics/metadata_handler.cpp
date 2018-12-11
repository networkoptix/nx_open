#include "metadata_handler.h"

#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>

#include <plugins/plugin_tools.h>
#include <nx/mediaserver_plugins/utils/uuid.h>

#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <core/resource/security_cam_resource.h>
#include <nx/vms/server/event/event_connector.h>
#include <analytics/common/object_detection_metadata.h>
#include <nx/fusion/model_functions.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <media_server/media_server_module.h>

#include <core/resource/camera_resource.h>

namespace nx {
namespace vms::server {
namespace analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

MetadataHandler::MetadataHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
    connect(this, &MetadataHandler::sdkEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_analyticsSdkEvent,
        Qt::QueuedConnection);
}

nx::vms::api::analytics::EventType MetadataHandler::eventTypeDescriptor(
    const QString& eventTypeId) const
{
    for (const auto& entry: m_eventTypeDescriptors)
    {
        const auto& id = entry.first;
        const auto& descriptor = entry.second;
        if (id == eventTypeId)
            return descriptor.item;
    }
    return nx::vms::api::analytics::EventType();
}

void MetadataHandler::handleMetadata(IMetadataPacket* metadata)
{
    if (metadata == nullptr)
    {
        NX_VERBOSE(this) << "WARNING: Received null metadata packet; ignoring";
        return;
    }

    bool handled = false;
    nxpt::ScopedRef<IEventMetadataPacket> eventsPacket(
        metadata->queryInterface(IID_EventMetadataPacket));
    if (eventsPacket)
    {
        handleEventsPacket(std::move(eventsPacket));
        handled = true;
    }

    nxpt::ScopedRef<IObjectMetadataPacket> objectsPacket(
        metadata->queryInterface(IID_ObjectMetadataPacket));
    if (objectsPacket)
    {
        handleObjectsPacket(std::move(objectsPacket));
        handled = true;
    }

    if (!handled)
    {
        NX_VERBOSE(this) << "WARNING: Received unsupported metadata packet with timestampUs "
            << metadata->timestampUs() << ", durationUs " << metadata->durationUs()
            << "; ignoring";
    }
}

void MetadataHandler::handleEventsPacket(nxpt::ScopedRef<IEventMetadataPacket> packet)
{
    int eventsCount = 0;
    while (true)
    {
        nxpt::ScopedRef<IMetadataItem> item(packet->nextItem(), /*increaseRef*/ false);
        if (!item)
            break;

        ++eventsCount;

        nxpt::ScopedRef<IEvent> eventData(item->queryInterface(IID_Event));
        if (eventData)
        {
            const int64_t timestampUsec = packet->timestampUs();
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

void MetadataHandler::handleObjectsPacket(nxpt::ScopedRef<IObjectMetadataPacket> packet)
{
    using namespace nx::mediaserver_plugins::utils;
    nx::common::metadata::DetectionMetadataPacket data;
    while (true)
    {
        nxpt::ScopedRef<IObject> item(packet->nextItem(), /*increaseRef*/ false);
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
    data.timestampUsec = packet->timestampUs();
    data.durationUsec = packet->durationUs();
    data.deviceId = m_resource->getId();

    if (data.timestampUsec <= 0)
        NX_WARNING(this, "Invalid ObjectsMetadataPacket timestamp: %1", data.timestampUsec);

    if (m_metadataSink && data.timestampUsec >= 0) //< Warn about 0 but still accept it.
        m_metadataSink->putData(nx::common::metadata::toMetadataPacket(data));

    if (m_visualDebugger)
        m_visualDebugger->push(nx::common::metadata::toMetadataPacket(data));
}

void MetadataHandler::handleMetadataEvent(
    nxpt::ScopedRef<IEvent> eventData,
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
        m_pluginId,
        eventTypeId,
        eventState,
        eventData->caption(),
        eventData->description(),
        eventData->auxiliaryData(),
        timestampUsec);

    if (m_resource->captureEvent(sdkEvent))
    {
        NX_VERBOSE(this) << __func__ << lm("(): Capturing event");
        return;
    }

    emit sdkEventTriggered(sdkEvent);
}

void MetadataHandler::setResource(QnVirtualCameraResourcePtr resource)
{
    m_resource = std::move(resource);
}

void MetadataHandler::setPluginId(QString pluginId)
{
    m_pluginId = std::move(pluginId);
}

void MetadataHandler::setEventTypeDescriptors(DescriptorMap descriptors)
{
    m_eventTypeDescriptors = std::move(descriptors);
}

void MetadataHandler::setMetadataSink(QnAbstractDataReceptor* dataReceptor)
{
    m_metadataSink = dataReceptor;
}

void MetadataHandler::removeMetadataSink(QnAbstractDataReceptor* dataReceptor)
{
    m_metadataSink = nullptr;
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

} // namespace analytics
} // namespace vms::server
} // namespace nx
