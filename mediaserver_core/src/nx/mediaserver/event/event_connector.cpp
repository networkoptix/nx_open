#include "event_connector.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/mediaserver/event/rule_processor.h>
#include <nx/vms/event/actions/actions_fwd.h>
#include <nx/vms/event/actions/system_health_action.h>
#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/events.h>
#include <media_server/serverutil.h>

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

        connect(camera.data(), &QnSecurityCamResource::analyticsEventStart,
            this, &EventConnector::at_analyticsEventStart);

        connect(camera.data(), &QnSecurityCamResource::analyticsEventEnd,
            this, &EventConnector::at_analyticsEventEnd);
    }
}

void EventConnector::at_motionDetected(const QnResourcePtr& resource, bool value,
    qint64 timeStamp, const QnConstAbstractDataPacketPtr& metadata)
{
    vms::event::MotionEventPtr event(new vms::event::MotionEvent(
        resource,
        value ? vms::event::EventState::active : vms::event::EventState::inactive,
        timeStamp,
        metadata));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_cameraDisconnected(const QnResourcePtr& resource, qint64 timeStamp)
{
    vms::event::CameraDisconnectedEventPtr event(
        new vms::event::CameraDisconnectedEvent(resource, timeStamp));
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

    vms::event::StorageFailureEventPtr event(new vms::event::StorageFailureEvent(
        server, timeStamp, reasonCode, url));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_storageFailure(const QnResourcePtr& server, qint64 timeStamp,
    nx::vms::event::EventReason reasonCode, const QString& storageUrl)
{
    QString url = storageUrl;
    if (url.contains(lit("://")))
        url = QUrl(url).toString(QUrl::RemoveUserInfo);

    vms::event::StorageFailureEventPtr event(new vms::event::StorageFailureEvent(
        server, timeStamp, reasonCode, url));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_serverFailure(const QnResourcePtr& resource, qint64 timeStamp,
    vms::event::EventReason reasonCode, const QString& reasonText)
{
    vms::event::ServerFailureEventPtr event(new vms::event::ServerFailureEvent(
        resource, timeStamp, reasonCode, reasonText));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_licenseIssueEvent(const QnResourcePtr& resource,
    qint64 timeStamp, vms::event::EventReason reasonCode, const QString& reasonText)
{
    vms::event::LicenseIssueEventPtr event(new vms::event::LicenseIssueEvent(
        resource, timeStamp, reasonCode, reasonText));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_serverStarted(const QnResourcePtr& resource, qint64 timeStamp)
{
    vms::event::ServerStartedEventPtr event(new vms::event::ServerStartedEvent(
        resource, timeStamp));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_cameraIPConflict(const QnResourcePtr& resource,
    const QHostAddress& hostAddress, const QStringList& macAddrList , qint64 timeStamp)
{
    vms::event::IpConflictEventPtr event(new vms::event::IpConflictEvent(
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
    vms::event::NetworkIssueEventPtr event(new vms::event::NetworkIssueEvent(
        resource, timeStamp, reasonCode, reasonParamsEncoded));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_cameraInput(const QnResourcePtr& resource,
    const QString& inputPortID, bool value, qint64 timeStampUsec)
{
    if (!resource)
        return;

    vms::event::CameraInputEventPtr event(new vms::event::CameraInputEvent(
        resource->toSharedPointer(),
        value ? vms::event::EventState::active : vms::event::EventState::inactive,
        timeStampUsec,
        inputPortID));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_analyticsEventStart(
    const QnResourcePtr& resource,
    const QString& caption,
    const QString& description,
    qint64 timestamp)
{
    auto secCam = resource.dynamicCast<QnSecurityCamResource>();
    if (!secCam)
        return;

    nx::vms::event::EventMetaData metadata;
    metadata.cameraRefs.push_back(secCam->getId());
    at_customEvent(
        secCam->getUserDefinedName(),
        caption,
        description,
        metadata,
        nx::vms::event::EventState::active,
        timestamp);
}

void EventConnector::at_analyticsEventEnd(
    const QnResourcePtr& resource,
    const QString& caption,
    const QString& description,
    qint64 timestamp)
{
    auto secCam = resource.dynamicCast<QnSecurityCamResource>();
    if (!secCam)
        return;

    nx::vms::event::EventMetaData metadata;
    metadata.cameraRefs.push_back(secCam->getId());
    at_customEvent(
        secCam->getUserDefinedName(),
        caption,
        description,
        metadata,
        nx::vms::event::EventState::inactive,
        timestamp);
}

void EventConnector::at_softwareTrigger(const QnResourcePtr& resource,
    const QString& triggerId, const QnUuid& userId, qint64 timeStamp,
    vms::event::EventState toggleState)
{
    if (!resource)
        return;

    vms::event::SoftwareTriggerEventPtr event(new vms::event::SoftwareTriggerEvent(
        resource->toSharedPointer(), triggerId, userId, timeStamp, toggleState));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_analyticsSdkEvent(const QnResourcePtr& resource,
    const QnUuid& driverId,
    const QnUuid& eventId,
    vms::event::EventState toggleState,
    const QString& caption,
    const QString& description,
    const QString& auxiliaryData,
    qint64 timeStampUsec)
{
    if (!resource)
        return;

    vms::event::AnalyticsSdkEventPtr event(new vms::event::AnalyticsSdkEvent(
        resource->toSharedPointer(),
        driverId,
        eventId,
        toggleState,
        caption,
        description,
        auxiliaryData,
        timeStampUsec));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_analyticsSdkEvent(const nx::vms::event::AnalyticsSdkEventPtr& event)
{
    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_customEvent(const QString& resourceName, const QString& caption,
    const QString& description, const vms::event::EventMetaData& metadata,
    vms::event::EventState eventState, qint64 timeStampUsec)
{
    vms::event::CustomEventPtr event(new vms::event::CustomEvent(
        eventState, timeStampUsec, resourceName, caption, description, metadata));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_serverConflict(const QnResourcePtr& resource, qint64 timeStamp,
    const QnCameraConflictList& conflicts)
{
    vms::event::ServerConflictEventPtr event(new vms::event::ServerConflictEvent(
        resource, timeStamp, conflicts));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_serverConflict(const QnResourcePtr& resource, qint64 timeStamp,
    const QnModuleInformation& conflictModule, const QUrl& url)
{
    vms::event::ServerConflictEventPtr event(new vms::event::ServerConflictEvent(
        resource, timeStamp, conflictModule, url));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_archiveBackupFinished(const QnResourcePtr& resource, qint64 timeStamp, vms::event::EventReason reasonCode, const QString& reasonText)
{
    vms::event::BackupFinishedEventPtr event(new vms::event::BackupFinishedEvent(
        resource, timeStamp, reasonCode, reasonText));

    qnEventRuleProcessor->processEvent(event);
}

void EventConnector::at_noStorages(const QnResourcePtr& resource)
{
    vms::event::SystemHealthActionPtr action(new vms::event::SystemHealthAction(
        QnSystemHealth::StoragesNotConfigured, resource->getId()));
    qnEventRuleProcessor->broadcastAction(action);
}

void EventConnector::at_remoteArchiveSyncStarted(const QnResourcePtr& resource)
{
    const auto secRes = resource.dynamicCast<QnSecurityCamResource>();
    NX_ASSERT(secRes, lit("Resource is not a descendant of QnSecurityCamResource"));
    if (!secRes)
        return;

    vms::event::SystemHealthActionPtr action(
        new vms::event::SystemHealthAction(QnSystemHealth::MessageType::RemoteArchiveSyncStarted,
            serverGuid()));

    auto params = action->getRuntimeParams();
    params.description = lit("Remote archive synchronization has been started for resource %1")
        .arg(secRes->getUserDefinedName());

    params.metadata.cameraRefs.push_back(resource->getId());
    action->setRuntimeParams(params);
    qnEventRuleProcessor->broadcastAction(action);
}

void EventConnector::at_remoteArchiveSyncFinished(const QnResourcePtr &resource)
{
    const auto secRes = resource.dynamicCast<QnSecurityCamResource>();
    NX_ASSERT(secRes, lit("Resource is not a descendant of QnSecurityCamResource"));
    if (!secRes)
        return;

    vms::event::SystemHealthActionPtr action(new vms::event::SystemHealthAction(
        QnSystemHealth::MessageType::RemoteArchiveSyncFinished,
        serverGuid()));

    auto params = action->getRuntimeParams();
    params.description = lit("Remote archive synchronization has been finished for resource %1")
        .arg(secRes->getUserDefinedName());

    params.metadata.cameraRefs.push_back(resource->getId());
    action->setRuntimeParams(params);
    qnEventRuleProcessor->broadcastAction(action);
}

void EventConnector::at_remoteArchiveSyncError(
    const QnResourcePtr &resource,
    const QString& error)
{
    const auto secRes = resource.dynamicCast<QnSecurityCamResource>();
    NX_ASSERT(secRes, lit("Resource is not a descendant of QnSecurityCamResource"));
    if (!secRes)
        return;

    vms::event::SystemHealthActionPtr action(new vms::event::SystemHealthAction(
        QnSystemHealth::MessageType::RemoteArchiveSyncError,
        serverGuid()));

    auto params = action->getRuntimeParams();
    params.metadata.cameraRefs.push_back(resource->getId());
    params.caption = error;
    params.description = lit("Error occurred while synchronizing remote archive for resource %1")
        .arg(secRes->getUserDefinedName());

    action->setRuntimeParams(params);
    qnEventRuleProcessor->broadcastAction(action);
}

void EventConnector::at_remoteArchiveSyncProgress(
    const QnResourcePtr &resource,
    double progress)
{
    NX_ASSERT(progress >= 0 && progress <= 1);

    const auto secRes = resource.dynamicCast<QnSecurityCamResource>();
    NX_ASSERT(secRes, lit("Resource is not a descendant of QnSecurityCamResource"));
    if (!secRes)
        return;

    vms::event::SystemHealthActionPtr action(new vms::event::SystemHealthAction(
        QnSystemHealth::MessageType::RemoteArchiveSyncProgress,
        serverGuid()));

    auto params = action->getRuntimeParams();
    params.metadata.cameraRefs.push_back(resource->getId());
    params.description = lit("Remote archive synchronization progress is %1% for resource %2")
        .arg(std::round(progress * 100))
        .arg(secRes->getUserDefinedName());

    action->setRuntimeParams(params);
    qDebug() << "Broadcasting sync progress business action";
    qnEventRuleProcessor->broadcastAction(action);
}

void EventConnector::at_archiveRebuildFinished(const QnResourcePtr& resource,
    QnSystemHealth::MessageType msgType)
{
    vms::event::SystemHealthActionPtr action(new vms::event::SystemHealthAction(
        msgType, resource->getId()));
    qnEventRuleProcessor->broadcastAction(action);
}

void EventConnector::at_fileIntegrityCheckFailed(const QnResourcePtr& resource)
{
    vms::event::SystemHealthActionPtr action(
        new vms::event::SystemHealthAction(
            QnSystemHealth::ArchiveIntegrityFailed,
            resource->getId()));
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
    const bool isOnState = eventState == vms::event::EventState::active;

    if (params.eventType >= vms::event::userDefinedEvent)
    {
        if (!check(!params.resourceName.isEmpty()
                || !params.caption.isEmpty()
                || !params.description.isEmpty(),
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
        case vms::event::cameraMotionEvent:
        {
            if (!check(resource, lit("'CameraMotionEvent' requires 'resource' parameter")))
                return false;

            at_motionDetected(resource, isOnState, params.eventTimestampUsec,
                QnConstAbstractDataPacketPtr());
            return true;
        }

        case vms::event::cameraInputEvent:
        {
            if (!check(resource, lit("'CameraInputEvent' requires 'resource' parameter")))
                return false;

            at_cameraInput(resource, params.inputPortId, isOnState, params.eventTimestampUsec);
            return true;
        }

        case vms::event::softwareTriggerEvent:
        {
            if (!check(resource, lit("'SoftwareTriggerEvent' requires 'resource' parameter")))
                return false;

            at_softwareTrigger(resource, params.inputPortId, userId,
                params.eventTimestampUsec, eventState);
            return true;
        }

        case vms::event::cameraDisconnectEvent:
        {
            if (!check(resource, lit("'CameraDisconnectEvent' requires 'resource' parameter")))
                return false;

            at_cameraDisconnected(resource, params.eventTimestampUsec);
            return true;
        }

        case vms::event::storageFailureEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_storageFailure(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::networkIssueEvent:
        {
            if (!check(resource, lit("'NetworkIssueEvent' requires 'resource' parameter")))
                return false;

            at_networkIssue(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::cameraIpConflictEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_cameraIPConflict(resource, QHostAddress(params.caption),
                params.description.split(nx::vms::event::IpConflictEvent::delimiter()),
                params.eventTimestampUsec);
            return true;
        }

        case vms::event::serverFailureEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_serverFailure(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::serverConflictEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            QnCameraConflictList conflicts;
            conflicts.decode(params.description);

            at_serverConflict(resource, params.eventTimestampUsec, conflicts);
            return true;
        }

        case vms::event::serverStartEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_serverStarted(resource, params.eventTimestampUsec);
            return true;
        }

        case vms::event::licenseIssueEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_licenseIssueEvent(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::backupFinishedEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);

            at_archiveBackupFinished(resource, params.eventTimestampUsec,
                params.reasonCode, params.description);
            return true;
        }

        case vms::event::analyticsSdkEvent:
        {
            if (!check(resource, lit("'AnalyticsSdkEvent' requires 'resource' parameter")))
                return false;

            at_analyticsSdkEvent(resource, params.analyticsDriverId(), params.analyticsEventId(),
                eventState,
                params.caption,
                params.description,
                /*auxiliary data*/QString(),
                params.eventTimestampUsec);

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
