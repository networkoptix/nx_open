#include "abstract_event.h"

#include "utils/common/synctime.h"
#include "core/resource/resource.h"
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/api/analytics/engine_manifest.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/descriptor_list_manager.h>

namespace nx {
namespace vms {
namespace event {

bool hasChild(EventType eventType)
{
    switch (eventType)
    {
        case EventType::anyCameraEvent:
        case EventType::anyServerEvent:
        case EventType::anyEvent:
            return true;
        default:
            return false;
    }
}

EventType parentEvent(EventType eventType)
{
    switch (eventType)
    {
        case EventType::cameraDisconnectEvent:
        case EventType::networkIssueEvent:
        case EventType::cameraIpConflictEvent:
            return EventType::anyCameraEvent;

        case EventType::storageFailureEvent:
        case EventType::serverFailureEvent:
        case EventType::serverConflictEvent:
        case EventType::serverStartEvent:
        case EventType::licenseIssueEvent:
        case EventType::backupFinishedEvent:
            return EventType::anyServerEvent;

        case EventType::anyEvent:
            return EventType::undefinedEvent;

        default:
            return EventType::anyEvent;
    }
}

QList<EventType> childEvents(EventType eventType)
{
    switch (eventType)
    {
        // Some critical issue occurred on the camera.
        case EventType::anyCameraEvent:
            return {
                EventType::cameraDisconnectEvent,
                EventType::networkIssueEvent,
                EventType::cameraIpConflictEvent,
            };

        // Some critical issue occurred on the server.
        case EventType::anyServerEvent:
            return {
                EventType::storageFailureEvent,
                EventType::serverFailureEvent,
                EventType::serverConflictEvent,
                EventType::serverStartEvent,
                EventType::licenseIssueEvent,
                EventType::backupFinishedEvent
            };

        // All events except already mentioned.
        case EventType::anyEvent:
            return {
                EventType::cameraMotionEvent,
                EventType::cameraInputEvent,
                EventType::softwareTriggerEvent,
                EventType::anyCameraEvent,
                EventType::anyServerEvent,
                EventType::analyticsSdkEvent,
                EventType::pluginEvent,
                EventType::userDefinedEvent
            };

        default:
            return {};
    }
}

QList<EventType> allEvents()
{
    static const QList<EventType> result {
        EventType::cameraMotionEvent,
        EventType::cameraInputEvent,
        EventType::cameraDisconnectEvent,
        EventType::storageFailureEvent,
        EventType::networkIssueEvent,
        EventType::cameraIpConflictEvent,
        EventType::serverFailureEvent,
        EventType::serverConflictEvent,
        EventType::serverStartEvent,
        EventType::licenseIssueEvent,
        EventType::backupFinishedEvent,
        EventType::softwareTriggerEvent,
        EventType::analyticsSdkEvent,
        EventType::pluginEvent,
        EventType::userDefinedEvent
    };

    return result;
}

bool isResourceRequired(EventType eventType)
{
    return requiresCameraResource(eventType)
        || requiresServerResource(eventType);
}

bool hasToggleState(
    EventType eventType,
    const EventParameters& runtimeParams,
    QnCommonModule* commonModule)
{
    switch (eventType)
    {
    case EventType::anyEvent:
    case EventType::cameraMotionEvent:
    case EventType::cameraInputEvent:
    case EventType::userDefinedEvent:
    case EventType::softwareTriggerEvent:
        return true;
    case EventType::analyticsSdkEvent:
    {
        if (runtimeParams.getAnalyticsEventTypeId().isNull())
            return true;

        auto descriptorListManager = commonModule->analyticsDescriptorListManager();
        const auto descriptor = descriptorListManager
            ->descriptor<nx::vms::api::analytics::EventTypeDescriptor>(
                runtimeParams.getAnalyticsEventTypeId());

        if (!descriptor)
            return false;

        return descriptor->item.flags.testFlag(nx::vms::api::analytics::stateDependent);
    }
    default:
        return false;
    }
}

QList<EventState> allowedEventStates(
    EventType eventType,
    const EventParameters& runtimeParams,
    QnCommonModule* commonModule)
{
    QList<EventState> result;
    const bool hasTooggleStateResult = hasToggleState(eventType, runtimeParams, commonModule);
    if (!hasTooggleStateResult
        || eventType == EventType::userDefinedEvent
        || eventType == EventType::softwareTriggerEvent)
        result << EventState::undefined;

    if (hasTooggleStateResult)
        result << EventState::active << EventState::inactive;
    return result;
}

// Check if camera required for this event to setup a rule. Camera selector will be displayed.
bool requiresCameraResource(EventType eventType)
{
    switch (eventType)
    {
        case EventType::cameraMotionEvent:
        case EventType::cameraInputEvent:
        case EventType::cameraDisconnectEvent: //< Think about moving out disconnect event.
        case EventType::softwareTriggerEvent:
        case EventType::analyticsSdkEvent:
        //case EventType::pluginEvent: //< Temporary disabled #spanasenko
            return true;

        default:
            return false;
    }
}

// Check if server required for this event to setup a rule. Server selector will be displayed.
bool requiresServerResource(EventType eventType)
{
    // TODO: #GDM #Business possibly will never be required.
    return false;
}

// Check if camera required for this event to OCCUR.
bool isSourceCameraRequired(EventType eventType)
{
    switch (eventType)
    {
        case EventType::networkIssueEvent:
            return true;

        default:
            return requiresCameraResource(eventType);
    }
}

// Check if server required for this event to OCCUR.
bool isSourceServerRequired(EventType eventType)
{
    switch (eventType)
    {
        case EventType::storageFailureEvent:
        case EventType::backupFinishedEvent:
        case EventType::serverFailureEvent:
        case EventType::serverConflictEvent:
        case EventType::serverStartEvent:
            return true;

        default:
            return requiresServerResource(eventType);
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
    NX_VERBOSE(this, "Created with type [%1] for resource [%2] at [%3]us",
        eventType, resource, timeStampUsec);
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
