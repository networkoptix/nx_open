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
        case anyCameraEvent:
            return true;
        case anyServerEvent:
            return true;
        case anyEvent:
            return true;
        default:
            return false;
    }
}

EventType parentEvent(EventType eventType)
{
    switch (eventType)
    {
        case cameraDisconnectEvent:
        case networkIssueEvent:
        case cameraIpConflictEvent:
            return anyCameraEvent;

        case storageFailureEvent:
        case serverFailureEvent:
        case serverConflictEvent:
        case serverStartEvent:
        case licenseIssueEvent:
        case backupFinishedEvent:
            return anyServerEvent;

        case anyEvent:
            return undefinedEvent;

        default:
            return anyEvent;
    }
}

QList<EventType> childEvents(EventType eventType)
{
    switch (eventType)
    {
        case anyCameraEvent:
            return {
                cameraDisconnectEvent,
                networkIssueEvent,
                cameraIpConflictEvent };

        case anyServerEvent:
            return {
                storageFailureEvent,
                serverFailureEvent,
                serverConflictEvent,
                serverStartEvent,
                licenseIssueEvent,
                backupFinishedEvent };

        case anyEvent:
            return {
                cameraMotionEvent,
                cameraInputEvent,
                anyCameraEvent,
                anyServerEvent,
                userDefinedEvent,
                softwareTriggerEvent };

        default:
            return {};
    }
}

QList<EventType> allEvents()
{
    static const QList<EventType> result {
        cameraMotionEvent,
        cameraInputEvent,
        cameraDisconnectEvent,
        storageFailureEvent,
        networkIssueEvent,
        cameraIpConflictEvent,
        serverFailureEvent,
        serverConflictEvent,
        serverStartEvent,
        licenseIssueEvent,
        backupFinishedEvent,
        softwareTriggerEvent,
        userDefinedEvent };

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
        case anyEvent:
        case cameraMotionEvent:
        case cameraInputEvent:
        case userDefinedEvent:
        case softwareTriggerEvent:
            return true;

        default:
            return false;
    }
}

QList<EventState> allowedEventStates(EventType eventType)
{
    QList<EventState> result;
    if (!hasToggleState(eventType) || eventType == userDefinedEvent || eventType == softwareTriggerEvent)
        result << EventState::undefined;
    if (hasToggleState(eventType))
        result << EventState::active << EventState::inactive;
    return result;
}

bool requiresCameraResource(EventType eventType)
{
    switch (eventType)
    {
        case cameraMotionEvent:
        case cameraInputEvent:
        case cameraDisconnectEvent:
        case softwareTriggerEvent:
            return true;

        default:
            return false;
    }
}

bool requiresServerResource(EventType eventType)
{
    switch (eventType)
    {
        case storageFailureEvent:
            return false; // TODO: #GDM #Business restore when will work fine
        default:
            return false;
    }
    return false;
}

bool isSourceCameraRequired(EventType eventType)
{
    switch (eventType)
    {
        case cameraMotionEvent:
        case cameraInputEvent:
        case cameraDisconnectEvent:
        case networkIssueEvent:
        case softwareTriggerEvent:
            return true;

        default:
            return false;
    }
}

bool isSourceServerRequired(EventType eventType)
{
    switch (eventType)
    {
        case storageFailureEvent:
        case backupFinishedEvent:
        case serverFailureEvent:
        case serverConflictEvent:
        case serverStartEvent:
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
