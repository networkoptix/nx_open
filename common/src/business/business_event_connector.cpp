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

#include "core/resource/resource.h"
#include <core/resource_managment/resource_pool.h>
#include "health/system_health.h"
#include "actions/system_health_business_action.h"


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

void QnBusinessEventConnector::at_storageFailure(const QnResourcePtr &mServerRes, qint64 timeStamp, int reasonCode, const QnResourcePtr &storageRes)
{
    QnStorageFailureBusinessEventPtr storageEvent(new QnStorageFailureBusinessEvent(
        mServerRes,
        timeStamp,
        reasonCode,
        storageRes
    ));
    qnBusinessRuleProcessor->processBusinessEvent(storageEvent);
}

void QnBusinessEventConnector::at_mserverFailure(const QnResourcePtr &resource, qint64 timeStamp, int reasonCode)
{
    QnMServerFailureBusinessEventPtr mserverEvent(new QnMServerFailureBusinessEvent(
        resource,
        timeStamp,
        reasonCode));
    qnBusinessRuleProcessor->processBusinessEvent(mserverEvent);
}

void QnBusinessEventConnector::at_cameraIPConflict(const QnResourcePtr& resource, const QHostAddress& hostAddress, const QStringList& macAddrList , qint64 timeStamp)
{
    QnIPConflictBusinessEventPtr ipConflictEvent(new QnIPConflictBusinessEvent(
        resource,
        hostAddress,
        macAddrList,
        timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(ipConflictEvent);
}

void QnBusinessEventConnector::at_networkIssue(const QnResourcePtr &resource, qint64 timeStamp, int reasonCode, const QString &reasonText)
{
    QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(
        resource,
        timeStamp,
        reasonCode,
        reasonText));
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
                                   timeStamp*1000,
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

void QnBusinessEventConnector::at_NoStorages(qint64 timeStamp)
{
    QnPopupBusinessActionPtr action(new QnSystemHealthBusinessAction(QnSystemHealth::StoragesNotConfigured));
    qnBusinessRuleProcessor->showPopup(action);
}
