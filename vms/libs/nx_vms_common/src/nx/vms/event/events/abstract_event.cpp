// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/common/system_context.h>
#include <utils/common/synctime.h>

using namespace nx::vms::common;
using nx::analytics::taxonomy::AbstractState;
using nx::analytics::taxonomy::AbstractEventType;
using nx::analytics::taxonomy::AbstractGroup;
using nx::analytics::taxonomy::AbstractScope;

namespace nx {
namespace vms {
namespace event {

namespace {

static const QList<EventType> kAllEvents{
    EventType::cameraMotionEvent,
    EventType::cameraInputEvent,
    EventType::cameraDisconnectEvent,
    EventType::storageFailureEvent,
    EventType::networkIssueEvent,
    EventType::cameraIpConflictEvent,
    EventType::serverFailureEvent,
    EventType::serverConflictEvent,
    EventType::serverStartEvent,
    EventType::ldapSyncIssueEvent,
    EventType::licenseIssueEvent,
    EventType::backupFinishedEvent,
    EventType::poeOverBudgetEvent,
    EventType::fanErrorEvent,
    EventType::softwareTriggerEvent,
    EventType::analyticsSdkEvent,
    EventType::analyticsSdkObjectDetected,
    EventType::pluginDiagnosticEvent,
    EventType::serverCertificateError,
    EventType::userDefinedEvent
};

/**
 * User should not be able to create these events, but should be able to view them in the event
 * log.
 */
static const QList<EventType> kDeprecatedEvents {
    EventType::backupFinishedEvent
};

}

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
        case EventType::ldapSyncIssueEvent:
        case EventType::licenseIssueEvent:
        case EventType::backupFinishedEvent:
        case EventType::poeOverBudgetEvent:
        case EventType::fanErrorEvent:
        case EventType::serverCertificateError:
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
                EventType::ldapSyncIssueEvent,
                EventType::licenseIssueEvent,
                EventType::backupFinishedEvent,
                EventType::poeOverBudgetEvent,
                EventType::fanErrorEvent,
                EventType::serverCertificateError
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
                EventType::analyticsSdkObjectDetected,
                EventType::pluginDiagnosticEvent,
                EventType::userDefinedEvent
            };

        default:
            return {};
    }
}

QList<EventType> allEvents(bool includeDeprecated)
{
    if (includeDeprecated)
        return kAllEvents;

    const auto result = nx::utils::toQSet(kAllEvents)
        .subtract(nx::utils::toQSet(kDeprecatedEvents));

    return result.values();
}

bool isResourceRequired(EventType eventType)
{
    return requiresCameraResource(eventType)
        || requiresServerResource(eventType);
}

bool isEventGroupContainsOnlyProlongedEvents(
    const std::shared_ptr<AbstractState>& taxonomyState,
    const QString& groupId)
{
    const auto& eventTypes = taxonomyState->eventTypes();
    if (eventTypes.empty())
        return false;

    for (const auto& eventType: eventTypes)
    {
        for (const auto& scope: eventType->scopes())
        {
            const AbstractGroup* group = scope->group();
            if (group && (group->id() == groupId))
            {
                if (!eventType->isStateDependent())
                    return false;
            }
        }
    }
    return true;
}

bool hasToggleState(
    EventType eventType,
    const EventParameters& runtimeParams,
    SystemContext* systemContext)
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

        const std::shared_ptr<AbstractState> taxonomyState =
            systemContext->analyticsTaxonomyState();

        if (!NX_ASSERT(taxonomyState))
            return false;

        const AbstractEventType* eventType = taxonomyState->eventTypeById(
            runtimeParams.getAnalyticsEventTypeId());

        if (eventType)
            return eventType->isStateDependent();

        return isEventGroupContainsOnlyProlongedEvents(
            taxonomyState, runtimeParams.getAnalyticsEventTypeId());
    }
    case EventType::analyticsSdkObjectDetected:
        return false;
    default:
        return false;
    }
}

QList<EventState> allowedEventStates(
    EventType eventType,
    const EventParameters& runtimeParams,
    nx::vms::common::SystemContext* systemContext)
{
    QList<EventState> result;
    const bool hasTooggleStateResult = hasToggleState(eventType, runtimeParams, systemContext);
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
        case EventType::analyticsSdkObjectDetected:
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
        case EventType::serverCertificateError:
            return true;

        default:
            return requiresServerResource(eventType);
    }
}

std::optional<QnResourceList> sourceResources(
    const EventParameters& params, QnResourcePool* resourcePool,
    const std::function<void(const QString&)>& notFound)
{
    if (params.eventResourceId.isNull() && params.metadata.cameraRefs.empty())
        return std::nullopt;

    QnResourceList result;
    for (const auto& ref: params.metadata.cameraRefs)
    {
        if (auto r = camera_id_helper::findCameraByFlexibleId(resourcePool, ref))
        {
            result.push_back(std::move(r));
        }
        else
        {
            NX_DEBUG(NX_SCOPE_TAG, "Unable to find event %1 resource ref %2", params.eventType, ref);
            if (notFound)
                notFound(ref);
        }
    }

    if (const auto& id = params.eventResourceId; !id.isNull())
    {
        if (auto r = resourcePool->getResourceById(id))
        {
            result.push_back(std::move(r));
        }
        else
        {
            NX_DEBUG(NX_SCOPE_TAG, "Unable to find event %1 resource id %2", params.eventType, id);
            if (notFound)
                notFound(id.toString());
        }
    }

    return result;
}

bool hasAccessToSource(const EventParameters& params, const QnUserResourcePtr& user)
{
    if (!user || !user->systemContext())
        return false;

    const auto context = user->systemContext();

    if (params.eventType == EventType::cameraIpConflictEvent)
    {
        const auto permission = Qn::WritePermission;
        for (const auto& ref: params.metadata.cameraRefs)
        {
            auto camera = camera_id_helper::findCameraByFlexibleId(context->resourcePool(), ref);
            if (!camera)
            {
                NX_DEBUG(NX_SCOPE_TAG,
                    "Unable to find event %1 resource ref %2", params.eventType, ref);
                continue;
            }
            if (context->resourceAccessManager()->hasPermission(user, camera, permission))
                return true;
        }

        NX_VERBOSE(NX_SCOPE_TAG,
            "User %1 has no permission %2 for the event %3", user, permission, params.eventType);
        return false;
    }

    if (isSourceCameraRequired(params.eventType))
    {
        const auto camera = context->resourcePool()->getResourceById(params.eventResourceId)
            .dynamicCast<QnVirtualCameraResource>();
        // The camera could be removed by user manually.
        if (!camera)
            return false;

        const auto hasPermission = context->resourceAccessManager()->hasPermission(
            user, camera, Qn::ViewContentPermission);
        NX_VERBOSE(NX_SCOPE_TAG, "%1 %2 permission for the event from the camera %3",
            user, hasPermission ? "has" : "has not", camera);
        return hasPermission;
    }

    if (isSourceServerRequired(params.eventType))
    {
        const auto server = context->resourcePool()->getResourceById(params.eventResourceId)
            .dynamicCast<QnMediaServerResource>();
        if (!server)
        {
            NX_WARNING(NX_SCOPE_TAG, "Event has occurred without its server %1", params.eventResourceId);
            return false;
        }

        // Only admins should see notifications with servers.
        const auto hasPermission = context->resourceAccessManager()->hasPowerUserPermissions(user);
        NX_VERBOSE(NX_SCOPE_TAG, "%1 %2 permission for the event from the server %3",
            user, hasPermission ? "has" : "has not", server);
        return hasPermission;
    }

    const auto resources = sourceResources(params, context->resourcePool());
    if (!resources)
    {
        NX_VERBOSE(NX_SCOPE_TAG, "%1 has permission for the event with no source", user);
        return true;
    }

    for (const auto& resource: *resources)
    {
        const auto requiredPermission = QnResourceAccessFilter::isShareableMedia(resource)
            ? Qn::ViewContentPermission
            : Qn::ReadPermission;

        if (context->resourceAccessManager()->hasPermission(user, resource, requiredPermission))
        {
            NX_VERBOSE(NX_SCOPE_TAG, "%1 has permission for the event from %2", user, resource);
            return true;
        }
    }

    NX_VERBOSE(NX_SCOPE_TAG, "%1 has not permission for the event from %2",
        user, containerString(*resources));
    return false;
}

AbstractEvent::AbstractEvent(
    EventType eventType,
    const QnResourcePtr& resource,
    EventState toggleState,
    qint64 timeStampUsec)
    :
    m_eventType(eventType),
    m_timeStamp(timeStampUsec),
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
    params.eventTimestampUsec = m_timeStamp.count();
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

QString AbstractEvent::getExternalUniqueKey() const
{
    return QString();
}

} // namespace event
} // namespace vms
} // namespace nx
