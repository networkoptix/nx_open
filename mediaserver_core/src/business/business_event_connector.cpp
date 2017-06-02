#include "business_event_connector.h"
#include "business_rule_processor.h"

#include <business/events/motion_business_event.h>
#include <business/events/camera_input_business_event.h>
#include <business/events/camera_disconnected_business_event.h>
#include <business/events/storage_failure_business_event.h>
#include <business/events/network_issue_business_event.h>
#include <business/events/mserver_failure_business_event.h>
#include <business/events/ip_conflict_business_event.h>
#include <business/events/mserver_conflict_business_event.h>
#include <business/events/software_trigger_business_event.h>

#include "core/resource/resource.h"
#include <core/resource_management/resource_pool.h>
#include "business/actions/system_health_business_action.h"
#include "core/resource/camera_resource.h"
#include "business/events/custom_business_event.h"
#include "business/events/conflict_business_event.h"

//#define REDUCE_NET_ISSUE_HACK

QnBusinessEventConnector::QnBusinessEventConnector(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &QnBusinessEventConnector::onNewResource);
}

QnBusinessEventConnector::~QnBusinessEventConnector()
{
}

void QnBusinessEventConnector::onNewResource(const QnResourcePtr &resource)
{
    QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource>(resource);
    if (camera)
    {
        connect(camera.data(), &QnSecurityCamResource::networkIssue, this, &QnBusinessEventConnector::at_networkIssue );
        connect(camera.data(), &QnSecurityCamResource::cameraInput, this, &QnBusinessEventConnector::at_cameraInput );
    }
}

void QnBusinessEventConnector::at_motionDetected(const QnResourcePtr &resource, bool value, qint64 timeStamp, const QnConstAbstractDataPacketPtr& metadata)
{
    QnMotionBusinessEventPtr motionEvent(new QnMotionBusinessEvent(resource, value ? QnBusiness::ActiveState : QnBusiness::InactiveState, timeStamp, metadata));
    qnBusinessRuleProcessor->processBusinessEvent(motionEvent);
}

void QnBusinessEventConnector::at_cameraDisconnected(const QnResourcePtr &resource, qint64 timeStamp)
{
    QnCameraDisconnectedBusinessEventPtr cameraEvent(new QnCameraDisconnectedBusinessEvent(resource, timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(cameraEvent);
}

void QnBusinessEventConnector::at_storageFailure(const QnResourcePtr &mServerRes, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QnResourcePtr &storageRes)
{
    QString url;
    if (storageRes)
        url = storageRes->getUrl();
    if (url.contains(lit("://")))
        url = QUrl(url).toString(QUrl::RemoveUserInfo);
    QnStorageFailureBusinessEventPtr storageEvent(new QnStorageFailureBusinessEvent(mServerRes, timeStamp, reasonCode, url));
    qnBusinessRuleProcessor->processBusinessEvent(storageEvent);
}

void QnBusinessEventConnector::at_storageFailure(const QnResourcePtr &mServerRes, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString &storageUrl)
{
    QString url = storageUrl;
    if (url.contains(lit("://")))
        url = QUrl(url).toString(QUrl::RemoveUserInfo);
    QnStorageFailureBusinessEventPtr storageEvent(new QnStorageFailureBusinessEvent(mServerRes, timeStamp, reasonCode, url));
    qnBusinessRuleProcessor->processBusinessEvent(storageEvent);
}

void QnBusinessEventConnector::at_mserverFailure(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText)
{
    QnMServerFailureBusinessEventPtr mserverEvent(new QnMServerFailureBusinessEvent(resource, timeStamp, reasonCode, reasonText));
    qnBusinessRuleProcessor->processBusinessEvent(mserverEvent);
}

void QnBusinessEventConnector::at_licenseIssueEvent(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText)
{
    QnLicenseIssueBusinessEventPtr bEvent(new QnLicenseIssueBusinessEvent(resource, timeStamp, reasonCode, reasonText));
    qnBusinessRuleProcessor->processBusinessEvent(bEvent);
}

void QnBusinessEventConnector::at_mserverStarted(const QnResourcePtr &resource, qint64 timeStamp)
{
    QnMServerStartedBusinessEventPtr mserverEvent(new QnMServerStartedBusinessEvent(resource, timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(mserverEvent);
}

void QnBusinessEventConnector::at_cameraIPConflict(const QnResourcePtr& resource, const QHostAddress& hostAddress, const QStringList& macAddrList , qint64 timeStamp)
{
    QnIPConflictBusinessEventPtr ipConflictEvent(new QnIPConflictBusinessEvent(resource, hostAddress, macAddrList, timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(ipConflictEvent);
}

void QnBusinessEventConnector::at_networkIssue(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString &reasonParamsEncoded)
{
#ifdef REDUCE_NET_ISSUE_HACK
    static int netIssueCounter;
    if (++netIssueCounter % 10)
        return; // mutex is not nessessary here
#endif
    QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(resource, timeStamp, reasonCode, reasonParamsEncoded));
    qnBusinessRuleProcessor->processBusinessEvent(networkEvent);
}

void QnBusinessEventConnector::at_cameraInput(const QnResourcePtr &resource, const QString& inputPortID, bool value, qint64 timeStampUsec)
{
    if( !resource )
        return;

    qnBusinessRuleProcessor->processBusinessEvent(
        QnCameraInputEventPtr(new QnCameraInputEvent(resource->toSharedPointer(), value ? QnBusiness::ActiveState : QnBusiness::InactiveState, timeStampUsec, inputPortID))
    );
}

void QnBusinessEventConnector::at_softwareTrigger(const QnResourcePtr& resource,
    const QString& triggerId, const QnUuid& userId, qint64 timeStamp,
    QnBusiness::EventState toggleState)
{
    if (!resource)
        return;

    QnSoftwareTriggerEventPtr triggerEvent(new QnSoftwareTriggerEvent(resource->toSharedPointer(),
        triggerId, userId, timeStamp, toggleState));

    qnBusinessRuleProcessor->processBusinessEvent(triggerEvent);
}

void QnBusinessEventConnector::at_customEvent(const QString &resourceName, const QString& caption, const QString& description,
                                              const QnEventMetaData& metadata, QnBusiness::EventState eventState, qint64 timeStampUsec)
{
    QnCustomBusinessEventPtr customEvent(new QnCustomBusinessEvent(eventState, timeStampUsec, resourceName, caption, description, metadata));
    qnBusinessRuleProcessor->processBusinessEvent(customEvent);
}

void QnBusinessEventConnector::at_mediaServerConflict(const QnResourcePtr& resource, qint64 timeStamp, const QnCameraConflictList& conflicts)
{
    QnMServerConflictBusinessEventPtr conflictEvent(new QnMServerConflictBusinessEvent(resource, timeStamp, conflicts));
    qnBusinessRuleProcessor->processBusinessEvent(conflictEvent);
}

void QnBusinessEventConnector::at_mediaServerConflict(const QnResourcePtr &resource, qint64 timeStamp, const QnModuleInformation &conflictModule, const QUrl &url)
{
    QnMServerConflictBusinessEventPtr conflictEvent(new QnMServerConflictBusinessEvent(resource, timeStamp, conflictModule, url));
    qnBusinessRuleProcessor->processBusinessEvent(conflictEvent);
}

void QnBusinessEventConnector::at_NoStorages(const QnResourcePtr& resource)
{
    QnAbstractBusinessActionPtr action(new QnSystemHealthBusinessAction(QnSystemHealth::StoragesNotConfigured, resource->getId()));
    qnBusinessRuleProcessor->broadcastBusinessAction(action);
}

void QnBusinessEventConnector::at_archiveRebuildFinished(const QnResourcePtr &resource, QnSystemHealth::MessageType msgType)
{
    QnAbstractBusinessActionPtr action(new QnSystemHealthBusinessAction(msgType, resource->getId()));
    qnBusinessRuleProcessor->broadcastBusinessAction(action);
}

void QnBusinessEventConnector::at_archiveBackupFinished(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText)
{
    QnBackupFinishedBusinessEventPtr bEvent(new QnBackupFinishedBusinessEvent(resource, timeStamp, reasonCode, reasonText));
    qnBusinessRuleProcessor->processBusinessEvent(bEvent);
}

bool QnBusinessEventConnector::createEventFromParams(const QnBusinessEventParameters& params,
    QnBusiness::EventState eventState, const QnUuid& userId, QString* errMessage)
{
    QnResourcePtr resource = resourcePool()->getResourceById(params.eventResourceId);
    bool isOnState = eventState == QnBusiness::ActiveState;
    if (params.eventType >= QnBusiness::UserDefinedEvent)
    {
        if (params.resourceName.isEmpty() && params.caption.isEmpty() &&  params.description.isEmpty()) {
            if (errMessage)
                *errMessage = "At least one of values 'source', 'caption' or 'description' should be filled";
            return false;
        }
        at_customEvent(params.resourceName, params.caption, params.description, params.metadata, eventState, params.eventTimestampUsec);
        return true;
    }

    switch (params.eventType)
    {
        case QnBusiness::CameraMotionEvent:
            if (!resource) {
                if (errMessage)
                    *errMessage = "'CameraMotionEvent' requires 'resource' parameter";
                return false;
            }
            at_motionDetected(resource, isOnState, params.eventTimestampUsec, QnConstAbstractDataPacketPtr());
            break;
        case QnBusiness::CameraInputEvent:
            if (!resource) {
                if (errMessage)
                    *errMessage = "'CameraInputEvent' requires 'resource' parameter";
                return false;
            }
            at_cameraInput(resource, params.inputPortId, isOnState, params.eventTimestampUsec);
            break;
        case QnBusiness::SoftwareTriggerEvent:
            if (!resource) {
                if (errMessage)
                    *errMessage = "'SoftwareTriggerEvent' requires 'resource' parameter";
                return false;
            }
            at_softwareTrigger(resource, params.inputPortId, userId, params.eventTimestampUsec, eventState);
            break;
        case QnBusiness::CameraDisconnectEvent:
            if (!resource) {
                if (errMessage)
                    *errMessage = "'CameraDisconnectEvent' requires 'resource' parameter";
                return false;
            }
            at_cameraDisconnected(resource, params.eventTimestampUsec);
            break;
        case QnBusiness::StorageFailureEvent:
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);
            at_storageFailure(resource, params.eventTimestampUsec, params.reasonCode, params.description);
            break;
        case QnBusiness::NetworkIssueEvent:
            if (!resource) {
                if (errMessage)
                    *errMessage = "'NetworkIssueEvent' requires 'resource' parameter";
                return false;
            }
            at_networkIssue(resource, params.eventTimestampUsec, params.reasonCode, params.description);
            break;
        case QnBusiness::CameraIpConflictEvent:
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);
            at_cameraIPConflict(resource, QHostAddress(params.caption), params.description.split(QnConflictBusinessEvent::Delimiter), params.eventTimestampUsec);
            break;
        case QnBusiness::ServerFailureEvent:
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);
            at_mserverFailure(resource, params.eventTimestampUsec, params.reasonCode, params.description);
            break;
        case QnBusiness::ServerConflictEvent:
        {
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);
            QnCameraConflictList conflicts;
            conflicts.decode(params.description);
            at_mediaServerConflict(resource, params.eventTimestampUsec, conflicts);
            break;
        }
        case QnBusiness::ServerStartEvent:
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);
            at_mserverStarted(resource, params.eventTimestampUsec);
            break;
        case QnBusiness::LicenseIssueEvent:
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);
            at_licenseIssueEvent(resource, params.eventTimestampUsec, params.reasonCode, params.description);
            break;
        case QnBusiness::BackupFinishedEvent:
            if (!resource)
                resource = resourcePool()->getResourceById(params.sourceServerId);
            at_archiveBackupFinished(resource, params.eventTimestampUsec, params.reasonCode, params.description);
            break;
        default:
            if (errMessage)
                *errMessage = lit("Unknown event type '%1'").arg(params.eventType);
            return false;
    }
    return true;
}
