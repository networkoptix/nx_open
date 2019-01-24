#include "metadata_handler.h"

#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/vms_server_plugins/utils/uuid.h>

#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <core/resource/security_cam_resource.h>
#include <nx/vms/server/event/event_connector.h>
#include <analytics/common/object_detection_metadata.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <media_server/media_server_module.h>

#include <core/resource/camera_resource.h>

namespace nx {
namespace vms::server {
namespace analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms::api::analytics;

MetadataHandler::MetadataHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
    connect(this, &MetadataHandler::sdkEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_analyticsSdkEvent,
        Qt::QueuedConnection);
}

std::optional<EventTypeDescriptor> MetadataHandler::eventTypeDescriptor(
    const QString& eventTypeId) const
{
    if (const auto it = m_eventTypeDescriptors.find(eventTypeId);
        it != m_eventTypeDescriptors.cend())
    {
        return it->second;
    }

    return std::nullopt;
}

void MetadataHandler::handleMetadata(IMetadataPacket* metadataPacket)
{
    if (metadataPacket == nullptr)
    {
        NX_VERBOSE(this) << "WARNING: Received null metadata packet; ignoring";
        return;
    }

    bool handled = false;
    if (const auto eventsPacket = nxpt::queryInterfacePtr<IEventMetadataPacket>(metadataPacket,
        IID_EventMetadataPacket))
    {
        handleEventMetadataPacket(eventsPacket);
        handled = true;
    }

    if (const auto objectsPacket = nxpt::queryInterfacePtr<IObjectMetadataPacket>(metadataPacket,
        IID_ObjectMetadataPacket))
    {
        handleObjectMetadataPacket(objectsPacket);
        handled = true;
    }

    if (!handled)
    {
        NX_VERBOSE(this) << "WARNING: Received unsupported metadata packet with timestampUs "
            << metadataPacket->timestampUs() << ", durationUs " << metadataPacket->durationUs()
            << "; ignoring";
    }
}

void MetadataHandler::handleEventMetadataPacket(
    nx::sdk::Ptr<IEventMetadataPacket> eventMetadataPacket)
{
    if (eventMetadataPacket->count() <= 0)
    {
        NX_VERBOSE(this, "WARNING: Received empty event packet; ignoring");
        return;
    }

    for (int i = 0; i < eventMetadataPacket->count(); ++i)
    {
        nx::sdk::Ptr<const IEventMetadata> eventMetadata(eventMetadataPacket->at(i));
        if (!eventMetadata)
            break;

        handleEventMetadata(eventMetadata, eventMetadataPacket->timestampUs());
    }
}

void MetadataHandler::handleObjectMetadataPacket(
    nx::sdk::Ptr<IObjectMetadataPacket> objectMetadataPacket)
{
    nx::common::metadata::DetectionMetadataPacket data;
    for (int i = 0; i < objectMetadataPacket->count(); ++i)
    {
        nx::sdk::Ptr<const IObjectMetadata> item(objectMetadataPacket->at(i));
        if (!item)
            break;

        nx::common::metadata::DetectedObject object;
        object.objectTypeId = item->typeId();
        object.objectId = nx::vms_server_plugins::utils::fromSdkUuidToQnUuid(item->id());
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

    data.timestampUsec = objectMetadataPacket->timestampUs();
    data.durationUsec = objectMetadataPacket->durationUs();
    data.deviceId = m_resource->getId();

    if (data.timestampUsec <= 0)
        NX_WARNING(this, "Invalid ObjectsMetadataPacket timestamp: %1", data.timestampUsec);

    if (m_metadataSink && data.timestampUsec >= 0) //< Warn about 0 but still accept it.
        m_metadataSink->putData(nx::common::metadata::toMetadataPacket(data));

    if (m_visualDebugger)
        m_visualDebugger->push(nx::common::metadata::toMetadataPacket(data));
}

void MetadataHandler::handleEventMetadata(
    nx::sdk::Ptr<const IEventMetadata> eventMetadata,
    qint64 timestampUsec)
{
    auto eventState = nx::vms::api::EventState::undefined;

    const auto eventTypeId = eventMetadata->typeId();
    NX_VERBOSE(this) << __func__ << lm("(): typeId %1").args(eventTypeId);

    auto descriptor = eventTypeDescriptor(eventTypeId);
    if (!descriptor)
    {
        NX_WARNING(this, "Unable to find descriptor for %1", eventTypeId);
        return;
    }

    if (descriptor->flags.testFlag(nx::vms::api::analytics::EventTypeFlag::stateDependent))
    {
        eventState = eventMetadata->isActive()
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
        m_engineId,
        eventTypeId,
        eventState,
        eventMetadata->caption(),
        eventMetadata->description(),
        eventMetadata->auxiliaryData(),
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

void MetadataHandler::setEngineId(QnUuid engineId)
{
    m_engineId = std::move(engineId);
}

void MetadataHandler::setEventTypeDescriptors(DescriptorMap descriptors)
{
    m_eventTypeDescriptors = std::move(descriptors);
}

void MetadataHandler::setMetadataSink(QnAbstractDataReceptor* dataReceptor)
{
    m_metadataSink = dataReceptor;
}

void MetadataHandler::removeMetadataSink(QnAbstractDataReceptor* /*dataReceptor*/)
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
