#include "abstract_event.h"

#include "utils/common/synctime.h"
#include "core/resource/resource.h"

namespace nx {
namespace vms {
namespace event {

bool hasChild(EventType eventType)
{
    switch (eventType)
    {
        case AnyCameraEvent:
            return true;
        case AnyServerEvent:
            return true;
        case AnyEvent:
            return true;
        default:
            return false;
    }
}

EventType parentEvent(EventType eventType)
{
    switch (eventType)
    {
        case CameraDisconnectEvent:
        case NetworkIssueEvent:
        case CameraIpConflictEvent:
            return AnyCameraEvent;

        case StorageFailureEvent:
        case ServerFailureEvent:
        case ServerConflictEvent:
        case ServerStartEvent:
        case LicenseIssueEvent:
        case BackupFinishedEvent:
            return AnyServerEvent;

        case AnyEvent:
            return UndefinedEvent;

        default:
            return AnyEvent;
    }
}

QList<EventType> childEvents(EventType eventType)
{
    switch (eventType)
    {
        case AnyCameraEvent:
            return {
                CameraDisconnectEvent,
                NetworkIssueEvent,
                CameraIpConflictEvent,
                SoftwareTriggerEvent };

        case AnyServerEvent:
            return {
                StorageFailureEvent,
                ServerFailureEvent,
                ServerConflictEvent,
                ServerStartEvent,
                LicenseIssueEvent,
                BackupFinishedEvent };

        case AnyEvent:
            return {
                CameraMotionEvent,
                CameraInputEvent,
                AnyCameraEvent,
                AnyServerEvent,
                UserDefinedEvent,
                SoftwareTriggerEvent };

        default:
            return {};
    }
}

QList<EventType> allEvents()
{
    static const QList<EventType> result {
        CameraMotionEvent,
        CameraInputEvent,
        CameraDisconnectEvent,
        StorageFailureEvent,
        NetworkIssueEvent,
        CameraIpConflictEvent,
        ServerFailureEvent,
        ServerConflictEvent,
        ServerStartEvent,
        LicenseIssueEvent,
        BackupFinishedEvent,
        SoftwareTriggerEvent,
        UserDefinedEvent };

    return result;
}

bool isResourceRequired(EventType eventType)
{
    return requiresCameraResource(eventType)
        || requiresServerResource(eventType);
}

bool hasToggleState(EventType eventType)
{
    switch (eventType)
    {
        case AnyEvent:
        case CameraMotionEvent:
        case CameraInputEvent:
        case UserDefinedEvent:
        case SoftwareTriggerEvent:
            return true;

        default:
            return false;
    }
}

QList<EventState> allowedEventStates(EventType eventType)
{
    QList<EventState> result;
    if (!hasToggleState(eventType) || eventType == UserDefinedEvent || eventType == SoftwareTriggerEvent)
        result << UndefinedState;
    if (hasToggleState(eventType))
        result << ActiveState << InactiveState;
    return result;
}

bool requiresCameraResource(EventType eventType)
{
    switch (eventType)
    {
        case CameraMotionEvent:
        case CameraInputEvent:
        case CameraDisconnectEvent:
        case SoftwareTriggerEvent:
            return true;

        default:
            return false;
    }
}

bool requiresServerResource(EventType eventType)
{
    switch (eventType)
    {
        case StorageFailureEvent:
            return false; //TODO: #GDM #Business restore when will work fine
        default:
            return false;
    }
    return false;
}

bool isSourceCameraRequired(EventType eventType)
{
    switch (eventType)
    {
        case CameraMotionEvent:
        case CameraInputEvent:
        case CameraDisconnectEvent:
        case NetworkIssueEvent:
            return true;

        default:
            return false;
    }
}

bool isSourceServerRequired(EventType eventType)
{
    switch (eventType)
    {
        case StorageFailureEvent:
        case BackupFinishedEvent:
        case ServerFailureEvent:
        case ServerConflictEvent:
        case ServerStartEvent:
            return true;

        default:
            return false;
    }
}

AbstractEvent::AbstractEvent(
    EventType eventType,
    const QnResourcePtr& resource,
    EventState toggleState,
    qint64 timeStampUsec)
    :
    m_eventType(eventType),
    m_timeStampUsec(timeStampUsec),
    m_resource(resource),
    m_toggleState(toggleState)
{
}

AbstractEvent::~AbstractEvent()
{
}

EventParameters AbstractEvent::getRuntimeParams() const
{
    EventParameters params;
    params.eventType = m_eventType;
    params.eventTimestampUsec = m_timeStampUsec;
    params.eventResourceId = m_resource ? m_resource->getId() : QnUuid();
    return params;
}

EventParameters AbstractEvent::getRuntimeParamsEx(
    const EventParameters& /*ruleEventParams*/) const
{
    return getRuntimeParams();
}

bool AbstractEvent::checkEventParams(const EventParameters& /*params*/) const
{
    return true;
}

} // namespace event
} // namespace vms
} // namespace nx
