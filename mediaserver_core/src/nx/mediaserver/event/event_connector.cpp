#include "event_connector.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/mediaserver/event/rule_processor.h>
#include <nx/vms/event/actions/actions_fwd.h>
#include <nx/vms/event/actions/system_health_action.h>
#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/events.h>

//#define REDUCE_NET_ISSUE_HACK

namespace nx {
namespace mediaserver {
namespace event {

EventConnector::EventConnector(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &EventConnector::onNewResource);
}

EventConnector::~EventConnector()
{
}

void EventConnector::onNewResource(const QnResourcePtr& resource)
{
    if (auto camera = qSharedPointerDynamicCast<QnSecurityCamResource>(resource))
    {
        connect(camera.data(), &QnSecurityCamResource::networkIssue,
            this, &EventConnector::at_networkIssue);

        connect(camera.data(), &QnSecurityCamResource::cameraInput,
            this, &EventConnector::at_cameraInput);
    }
}

void EventConnector::at_motionDetected(const QnResourcePtr& resource, bool value,
    qint64 timeStamp, const QnConstAbstractDataPacketPtr& metadata)
{
    vms::event::MotionEventPtr event(new vms::event::MotionEvent(
        resource,
        value ? vms::event::ActiveState : vms::event::InactiveState,
        timeStamp,
        metadata));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_cameraDisconnected(const QnResourcePtr& resource, qint64 timeStamp)
{
    vms::event::CameraDisconnectedEventPtr event(
        new class vms::event::CameraDisconnectedEvent(resource, timeStamp));
    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_storageFailure(const QnResourcePtr& server, qint64 timeStamp,
    vms::event::EventReason reasonCode, const QnResourcePtr& storage)
{
    QString url;
    if (storage)
        url = storage->getUrl();
    if (url.contains(lit("://")))
        url = QUrl(url).toString(QUrl::RemoveUserInfo);

    vms::event::StorageFailureEventPtr event(new class vms::event::StorageFailureEvent(
        server, timeStamp, reasonCode, url));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_storageFailure(const QnResourcePtr& server, qint64 timeStamp,
    nx::vms::event::EventReason reasonCode, const QString& storageUrl)
{
    QString url = storageUrl;
    if (url.contains(lit("://")))
        url = QUrl(url).toString(QUrl::RemoveUserInfo);

    vms::event::StorageFailureEventPtr event(new class vms::event::StorageFailureEvent(
        server, timeStamp, reasonCode, url));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_serverFailure(const QnResourcePtr& resource, qint64 timeStamp,
    vms::event::EventReason reasonCode, const QString& reasonText)
{
    vms::event::ServerFailureEventPtr event(new class vms::event::ServerFailureEvent(
        resource, timeStamp, reasonCode, reasonText));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_licenseIssueEvent(const QnResourcePtr& resource,
    qint64 timeStamp, vms::event::EventReason reasonCode, const QString& reasonText)
{
    vms::event::LicenseIssueEventPtr event(new class vms::event::LicenseIssueEvent(
        resource, timeStamp, reasonCode, reasonText));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_serverStarted(const QnResourcePtr& resource, qint64 timeStamp)
{
    vms::event::ServerStartedEventPtr event(new class vms::event::ServerStartedEvent(
        resource, timeStamp));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_cameraIPConflict(const QnResourcePtr& resource,
    const QHostAddress& hostAddress, const QStringList& macAddrList , qint64 timeStamp)
{
    vms::event::IpConflictEventPtr event(new class vms::event::IpConflictEvent(
        resource, hostAddress, macAddrList, timeStamp));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_networkIssue(const QnResourcePtr& resource, qint64 timeStamp,
    vms::event::EventReason reasonCode, const QString &reasonParamsEncoded)
{
#ifdef REDUCE_NET_ISSUE_HACK
    static int netIssueCounter;
    if (++netIssueCounter % 10)
        return; // mutex is not nessessary here
#endif
    vms::event::NetworkIssueEventPtr event(new class vms::event::NetworkIssueEvent(
        resource, timeStamp, reasonCode, reasonParamsEncoded));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_cameraInput(const QnResourcePtr& resource,
    const QString& inputPortID, bool value, qint64 timeStampUsec)
{
    if (!resource)
        return;

    vms::event::CameraInputEventPtr event(new class vms::event::CameraInputEvent(
        resource->toSharedPointer(),
        value ? vms::event::ActiveState : vms::event::InactiveState,
        timeStampUsec,
        inputPortID));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_softwareTrigger(const QnResourcePtr& resource,
    const QString& triggerId, const QnUuid& userId, qint64 timeStamp,
    vms::event::EventState toggleState)
{
    if (!resource)
        return;

    vms::event::SoftwareTriggerEventPtr event(new class vms::event::SoftwareTriggerEvent(
        resource->toSharedPointer(), triggerId, userId, timeStamp, toggleState));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_customEvent(const QString& resourceName, const QString& caption,
    const QString& description, const vms::event::EventMetaData& metadata,
    vms::event::EventState eventState, qint64 timeStampUsec)
{
    vms::event::CustomEventPtr event(new class vms::event::CustomEvent(
        eventState, timeStampUsec, resourceName, caption, description, metadata));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_serverConflict(const QnResourcePtr& resource, qint64 timeStamp,
    const QnCameraConflictList& conflicts)
{
    vms::event::ServerConflictEventPtr event(new class vms::event::ServerConflictEvent(
        resource, timeStamp, conflicts));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_serverConflict(const QnResourcePtr& resource, qint64 timeStamp,
    const QnModuleInformation& conflictModule, const QUrl& url)
{
    vms::event::ServerConflictEventPtr event(new class vms::event::ServerConflictEvent(
        resource, timeStamp, conflictModule, url));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_archiveBackupFinished(const QnResourcePtr& resource, qint64 timeStamp, vms::event::EventReason reasonCode, const QString& reasonText)
{
    vms::event::BackupFinishedEventPtr event(new class vms::event::BackupFinishedEvent(
        resource, timeStamp, reasonCode, reasonText));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_noStorages(const QnResourcePtr& resource)
{
    vms::event::SystemHealthActionPtr action(new class vms::event::SystemHealthAction(
        QnSystemHealth::StoragesNotConfigured, resource->getId()));
    qnEventRuleProcessor->broadcastAction(action);
}

void EventConnector::at_archiveRebuildFinished(const QnResourcePtr& resource,
    QnSystemHealth::MessageType msgType)
{
    vms::event::SystemHealthActionPtr action(new class vms::event::SystemHealthAction(
        msgType, resource->getId()));
    qnEventRuleProcessor->broadcastAction(action);
}

bool EventConnector::createEventFromParams(const vms::event::EventParameters& params,
    vms::event::EventState eventState, const QnUuid& userId, QString* errorMessage)
{
    const auto check =
        [errorMessage](bool condition, const QString& message)
        {
            if (condition)
                return true;

            if (errorMessage)
                *errorMessage = message;

            return false;
        };

    auto resource = resourcePool()->getResourceById(params.eventResourceId);
    const bool isOnState = eventState == vms::event::ActiveState;

    if (params.eventType >= vms::event::UserDefinedEvent)
    {
        if (!check(params.resourceName.isEmpty() && params.caption.isEmpty()
                && params.description.isEmpty(),
            lit("At least one of values 'source', 'caption' or 'description' should be filled")))
        {
            return false;
        }

        at_customEvent(params.resourceName, params.caption, params.description,
            params.metadata, eventState, params.eventTimestampUsec);
        return true;
    }

    switch (params.eventType)
    {
        case vms::event::CameraMotionEvent:
        {
            if (!check(resource, lit("'CameraMotionEvent' requires 'resource' parameter")))
                return false;

            at_motionDetected(resource, isOnState, params.eventTimestampUsec,
                QnConstAbstractDataPacketPtr());
            return true;
        }

        case vms::event::CameraInputEvent:
        {
            if (!check(resource, lit("'CameraInputEvent' requires 'resource' parameter")))
                return false;

            at_cameraInput(resource, params.inputPortId, isOnState, params.eventTimestampUsec);
            return true;
        }

        case vms::event::SoftwareTriggerEvent:
        {
            if (!check(resource, lit("'SoftwareTriggerEvent' requires 'resource' parameter")))
                return false;

            at_softwareTrigger(resource, params.inputPortId, userId,
                params.eventTimestampUsec, eventState);
            return true;
        }

        case vms::event::CameraDisconnectEvent:
        {
            if (!check(resource, lit("'CameraDisconnectEvent' requires 'resource' parameter")))
                return false;

            at_cameraDisconnected(resource, params.eventTimestampUsec);
            return true;
        }

        case vms::event::StorageFailureEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_storageFailure(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::NetworkIssueEvent:
        {
            if (!check(resource, lit("'NetworkIssueEvent' requires 'resource' parameter")))
                return false;

            at_networkIssue(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::CameraIpConflictEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_cameraIPConflict(resource, QHostAddress(params.caption),
                params.description.split(nx::vms::event::IpConflictEvent::delimiter()),
                params.eventTimestampUsec);
            return true;
        }

        case vms::event::ServerFailureEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_serverFailure(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::ServerConflictEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            QnCameraConflictList conflicts;
            conflicts.decode(params.description);

            at_serverConflict(resource, params.eventTimestampUsec, conflicts);
            return true;
        }

        case vms::event::ServerStartEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_serverStarted(resource, params.eventTimestampUsec);
            return true;
        }

        case vms::event::LicenseIssueEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_licenseIssueEvent(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::BackupFinishedEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_archiveBackupFinished(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        default:
        {
            check(false, lit("Unknown event type '%1'").arg(params.eventType));
            return false;
        }
    }
}

} // namespace event
} // namespace mediaserver
} // namespace nx
