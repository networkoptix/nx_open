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
#include <core/resource_management/resource_pool.h>
#include "health/system_health.h"
#include "actions/system_health_business_action.h"


//#define REDUCE_NET_ISSUE_HACK

static QnBusinessEventConnector* _instance = NULL;

QnBusinessEventConnector::QnBusinessEventConnector()
{
}

QnBusinessEventConnector::~QnBusinessEventConnector()
{
}

void QnBusinessEventConnector::initStaticInstance( QnBusinessEventConnector* inst )
{
    _instance = inst;
}

QnBusinessEventConnector* QnBusinessEventConnector::instance()
{
    //return static_instance();
    return _instance;
}

void QnBusinessEventConnector::at_motionDetected(const QnResourcePtr &resource, bool value, qint64 timeStamp, QnConstAbstractDataPacketPtr metadata)
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
    QnStorageFailureBusinessEventPtr storageEvent(new QnStorageFailureBusinessEvent(mServerRes, timeStamp, reasonCode, storageRes));
    qnBusinessRuleProcessor->processBusinessEvent(storageEvent);
}

void QnBusinessEventConnector::at_mserverFailure(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode)
{
    QnMServerFailureBusinessEventPtr mserverEvent(new QnMServerFailureBusinessEvent(resource, timeStamp, reasonCode));
    qnBusinessRuleProcessor->processBusinessEvent(mserverEvent);
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

void QnBusinessEventConnector::at_cameraInput(const QnResourcePtr &resource, const QString& inputPortID, bool value, qint64 timeStamp)
{
    if( !resource )
        return;

    qnBusinessRuleProcessor->processBusinessEvent(
        QnCameraInputEventPtr(new QnCameraInputEvent(resource->toSharedPointer(), value ? QnBusiness::ActiveState : QnBusiness::InactiveState, timeStamp * 1000, inputPortID))
    );
}


void QnBusinessEventConnector::at_mediaServerConflict(const QnResourcePtr& resource, qint64 timeStamp, const QList<QByteArray>& otherServers)
{
    QnMServerConflictBusinessEventPtr conflictEvent(new QnMServerConflictBusinessEvent(resource, timeStamp, otherServers));
    qnBusinessRuleProcessor->processBusinessEvent(conflictEvent);
}

void QnBusinessEventConnector::at_NoStorages(const QnResourcePtr& resource)
{
    QnAbstractBusinessActionPtr action(new QnSystemHealthBusinessAction(QnSystemHealth::StoragesNotConfigured, resource->getId()));
    qnBusinessRuleProcessor->broadcastBusinessAction(action);
}

void QnBusinessEventConnector::at_archiveRebuildFinished(const QnResourcePtr &resource) {
    QnAbstractBusinessActionPtr action(new QnSystemHealthBusinessAction(QnSystemHealth::ArchiveRebuildFinished, resource->getId()));
    qnBusinessRuleProcessor->broadcastBusinessAction(action);
}
