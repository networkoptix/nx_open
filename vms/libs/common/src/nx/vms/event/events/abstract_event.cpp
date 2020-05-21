#include "abstract_event.h"

#include "utils/common/synctime.h"
#include "core/resource/resource.h"
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/descriptor_manager.h>

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
        case EventType::poeOverBudgetEvent:
        case EventType::fanErrorEvent:
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
                EventType::backupFinishedEvent,
                EventType::poeOverBudgetEvent,
                EventType::fanErrorEvent
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
                EventType::pluginDiagnosticEvent,
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
        EventType::poeOverBudgetEvent,
        EventType::fanErrorEvent,
        EventType::softwareTriggerEvent,
        EventType::analyticsSdkEvent,
        EventType::pluginDiagnosticEvent,
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
    case EventType::poeOverBudgetEvent:
        return true;
    case EventType::analyticsSdkEvent:
    {
        if (runtimeParams.getAnalyticsEventTypeId().isNull())
            return true;

        const auto descriptor = commonModule->analyticsEventTypeDescriptorManager()->descriptor(
            runtimeParams.getAnalyticsEventTypeId());

        if (!descriptor)
            return false;

        return descriptor->flags.testFlag(nx::vms::api::analytics::stateDependent);
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
        case EventType::pluginDiagnosticEvent:
            return true;

        default:
            return false;
    }
}

// Check if server required for this event to setup a rule. Server selector will be displayed.
bool requiresServerResource(EventType eventType)
{
    switch (eventType)
    {
        case EventType::poeOverBudgetEvent:
        case EventType::fanErrorEvent:
            return true;
        default:
            return false;
    }
}

// Check if camera required for this event to OCCUR.
bool isSourceCameraRequired(EventType eventType)
{
    switch (eventType)
    {
        case EventType::networkIssueEvent:
            return true;

        case EventType::pluginDiagnosticEvent:
            return false;

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

bool hasAccessToSource(const EventParameters& params, const QnUserResourcePtr& user)
{
    if (!user || !user->commonModule())
        return false;

    const auto context = user->commonModule();

    const auto eventType = params.eventType;

    const auto resource = context->resourcePool()->getResourceById(params.eventResourceId);
    const bool hasViewPermission = resource && context->resourceAccessManager()->hasPermission(
        user, resource, Qn::ViewContentPermission);

    if (isSourceCameraRequired(eventType))
    {
        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camera, "Event has occurred without its camera %1", params.eventResourceId);
        return camera && hasViewPermission;
    }

    if (isSourceServerRequired(eventType))
    {
        const auto server = resource.dynamicCast<QnMediaServerResource>();
        NX_ASSERT(server, "Event has occurred without its server %1", params.eventResourceId);
        /* Only admins should see notifications with servers. */
        return server && hasViewPermission;
    }

    return true;
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
