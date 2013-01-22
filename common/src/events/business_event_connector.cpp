#include "business_event_connector.h"
#include "motion_business_event.h"
#include "camera_input_business_event.h"
#include "core/resource/resource.h"
#include "business_rule_processor.h"
#include "camera_disconnected_business_event.h"
#include "storage_failure_business_event.h"
#include "network_issue_business_event.h"
#include "mserver_failure_business_event.h"
#include "ip_conflict_business_event.h"
#include "core/resource_managment/resource_pool.h"
#include "mserver_conflict_business_event.h"

Q_GLOBAL_STATIC(QnBusinessEventConnector, static_instance)

QnBusinessEventConnector* QnBusinessEventConnector::instance()
{
    return static_instance();
}

void QnBusinessEventConnector::at_motionDetected(const QnResourcePtr &resource, bool value, qint64 timeStamp, QnAbstractDataPacketPtr metadata)
{
    QnMotionBusinessEventPtr motionEvent(new QnMotionBusinessEvent(
                                             resource,
                                             value ? ToggleState::On : ToggleState::Off,
                                             timeStamp,
                                             metadata));
    qnBusinessRuleProcessor->processBusinessEvent(motionEvent);
}

void QnBusinessEventConnector::at_cameraDisconnected(const QnResourcePtr &resource, qint64 timeStamp)
{
    QnCameraDisconnectedBusinessEventPtr cameraEvent(new QnCameraDisconnectedBusinessEvent(
        resource,
        timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(cameraEvent);
}

void QnBusinessEventConnector::at_storageFailure(const QnResourcePtr &mServerRes, qint64 timeStamp, const QnResourcePtr &storageRes, const QString& reason)
{
    QnStorageFailureBusinessEventPtr storageEvent(new QnStorageFailureBusinessEvent(
        mServerRes,
        timeStamp,
        storageRes,
        reason));
    qnBusinessRuleProcessor->processBusinessEvent(storageEvent);
}

void QnBusinessEventConnector::at_mserverFailure(const QnResourcePtr &resource, qint64 timeStamp)
{
    QnMServerFailureBusinessEventPtr mserverEvent(new QnMServerFailureBusinessEvent(
        resource,
        timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(mserverEvent);
}

void QnBusinessEventConnector::at_cameraIPConflict(const QnResourcePtr& resource, const QHostAddress& hostAddress, const QnNetworkResourceList& cameras, qint64 timeStamp)
{
    QnIPConflictBusinessEventPtr ipConflictEvent(new QnIPConflictBusinessEvent(
        resource,
        hostAddress,
        cameras,
        timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(ipConflictEvent);
}

void QnBusinessEventConnector::at_networkIssue(const QnResourcePtr &resource, qint64 timeStamp, const QString& reason)
{
    QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(
        resource,
        timeStamp,
        reason));
    qnBusinessRuleProcessor->processBusinessEvent(networkEvent);
}

void QnBusinessEventConnector::at_cameraInput(const QnResourcePtr &resource, const QString& inputPortID, bool value, qint64 timeStamp)
{
    if( !resource )
        return;

    qnBusinessRuleProcessor->processBusinessEvent(
        QnCameraInputEventPtr( new QnCameraInputEvent(
                                   resource->toSharedPointer(),
                                   value ? ToggleState::On : ToggleState::Off,
                                   timeStamp,
                                   inputPortID)));
}


void QnBusinessEventConnector::at_mediaServerConflict(const QnResourcePtr& resource, qint64 timeStamp, const QList<QByteArray>& otherServers)
{
    QnMServerConflictBusinessEventPtr conflictEvent(new QnMServerConflictBusinessEvent(
        resource,
        timeStamp,
        otherServers));
    qnBusinessRuleProcessor->processBusinessEvent(conflictEvent);
}
