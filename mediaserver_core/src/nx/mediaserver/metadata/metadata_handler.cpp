#include "metadata_handler.h"

#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>

#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <core/resource/security_cam_resource.h>
#include <nx/mediaserver/event/event_connector.h>
#include <analytics/common/object_detection_metadata.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

void MetadataHandler::handleMetadata(
    Error error,
    MetadataPacket* metadata)
{
    if (metadata == nullptr)
        return;

    if (error != Error::noError)
        return;

    nxpt::ScopedRef<AbstractEventMetadataPacket> eventsPacket(
        metadata->queryInterface(IID_EventMetadataPacket));
    if (eventsPacket)
        handleEventsPacket(std::move(eventsPacket));

    nxpt::ScopedRef<AbstractObjectsMetadataPacket> objectsPacket(
        metadata->queryInterface(IID_DetectionMetadataPacket));
    if (objectsPacket)
        handleObjectsPacket(std::move(objectsPacket));
}

void MetadataHandler::handleEventsPacket(nxpt::ScopedRef<AbstractEventMetadataPacket> packet)
{
    while (true)
    {
        nxpt::ScopedRef<AbstractMetadataItem> item(packet->nextItem(), /*increaseRef*/ false);
        if (!item)
            break;

        nxpt::ScopedRef<AbstractDetectedEvent> eventData(item->queryInterface(IID_DetectedEvent));
        if (eventData)
        {
            auto timestampUsec = packet->timestampUsec();
            handleMetadataEvent(std::move(eventData), timestampUsec);
        }
        else
        {
            NX_VERBOSE(this) << "ERROR: Received event does not implement AbstractDetectedEvent";
        }
    }
}

void MetadataHandler::handleObjectsPacket(nxpt::ScopedRef<AbstractObjectsMetadataPacket> packet)
{
    nx::common::metadata::DetectionMetadataPacket data;
    while (true)
    {
        nxpt::ScopedRef<AbstractDetectedObject> item(packet->nextItem(), /*increaseRef*/ false);
        if (!item)
            break;
        nx::common::metadata::DetectedObject object;
        object.objectTypeId = nxpt::fromPluginGuidToQnUuid(item->typeId());
        object.objectId = nxpt::fromPluginGuidToQnUuid(item->id());
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

            NX_VERBOSE(this) << __func__ << "Attribute:" << attribute.name << attribute.value;
        }
        data.objects.push_back(std::move(object));
    }
    data.timestampUsec = packet->timestampUsec();
    data.durationUsec = packet->durationUsec();
    data.deviceId = m_resource->getId();

    if (m_dataReceptor)
        m_dataReceptor->putData(nx::common::metadata::toMetadataPacket(data));
}

void MetadataHandler::handleMetadataEvent(
    nxpt::ScopedRef<AbstractDetectedEvent> eventData,
    qint64 timestampUsec)
{
    const auto eventState = eventData->isActive()
        ? nx::vms::event::EventState::active
        : nx::vms::event::EventState::inactive;

    const auto eventTypeId = nxpt::fromPluginGuidToQnUuid(eventData->typeId());

    NX_VERBOSE(this) << __func__ << lm("(): typeId %1").args(eventTypeId);

    const bool duplicate = eventState == nx::vms::event::EventState::inactive
        && lastEventState(eventTypeId) == nx::vms::event::EventState::inactive;

    if (duplicate)
    {
        NX_VERBOSE(this) << __func__ << lm("(): Ignoring duplicate event");
        return;
    }

    setLastEventState(eventTypeId, eventState);

    auto sdkEvent = nx::vms::event::AnalyticsSdkEventPtr::create(
        m_resource,
        m_pluginId,
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

    qnEventRuleConnector->at_analyticsSdkEvent(sdkEvent);
}

int MetadataHandler::getParamValue(
    const char* paramName, char* valueBuf, int* valueBufSize) const
{
    // TODO: STUB: Always return a fixed value, or an error if paramName is null or empty.

    if (!paramName || !paramName[0])
        return NX_UNKNOWN_PARAMETER;

    static const char kStubValue[] = "Stub Value";
    if (*valueBufSize < sizeof(kStubValue))
    {
        *valueBufSize = sizeof(kStubValue);
        return NX_MORE_DATA;
    }

    strncpy(valueBuf, kStubValue, sizeof(kStubValue));
    return NX_NO_ERROR;
}

void MetadataHandler::setResource(const QnSecurityCamResourcePtr& resource)
{
    m_resource = resource;
}

void MetadataHandler::setPluginId(const QnUuid& pluginId)
{
    m_pluginId = pluginId;
}

void MetadataHandler::registerDataReceptor(QnAbstractDataReceptor* dataReceptor)
{
    m_dataReceptor = dataReceptor;
}

void MetadataHandler::removeDataReceptor(QnAbstractDataReceptor* dataReceptor)
{
    m_dataReceptor = nullptr;
}

nx::vms::event::EventState MetadataHandler::lastEventState(const QnUuid& eventId) const
{
    if (m_eventStateMap.contains(eventId))
        return m_eventStateMap[eventId];

    return nx::vms::event::EventState::inactive;
}

void MetadataHandler::setLastEventState(const QnUuid& eventId, nx::vms::event::EventState eventState)
{
    m_eventStateMap[eventId] = eventState;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
